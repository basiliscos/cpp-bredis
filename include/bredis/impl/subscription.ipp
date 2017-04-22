//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <algorithm>
#include <cassert>
#include <type_traits>

#include "common.ipp"

namespace bredis {

template <typename AsyncStream>
template <typename C>
void Subscription<AsyncStream>::push_command(const std::string &cmd,
                                             C &&contaier) {
    args_container_t args{};
    std::copy(contaier.begin(), contaier.end(), std::back_inserter(args));

    auto tuple = std::make_tuple(cmd, std::move(args));

    std::unique_lock<std::mutex> lock(tx_queue_mutex_);
    tx_queue_->emplace(std::move(tuple));
    lock.unlock();

    try_write();
}

template <typename AsyncStream> void Subscription<AsyncStream>::try_write() {
    if (tx_in_progress_.fetch_add(1) == 0) {
        std::lock_guard<std::mutex> guard(tx_queue_mutex_);
        if (!tx_queue_->empty()) {
            BREDIS_LOG_DEBUG("(a) tx qeue size: " << tx_queue_->size());
            auto write_queue = std::make_unique<tx_queue_t::element_type>();
            std::swap(write_queue, tx_queue_);
            write(*write_queue);
            BREDIS_LOG_DEBUG("(b) tx qeue size: " << tx_queue_->size());
        } else {
            --tx_in_progress_;
        }
    } else {
        --tx_in_progress_;
    }
}

template <typename AsyncStream> void Subscription<AsyncStream>::try_read() {
    BREDIS_LOG_DEBUG("try_read() rx_in_progress_ = " << rx_in_progress_);
    if (rx_in_progress_.fetch_add(1) == 0) {
        read();
    } else {
        --rx_in_progress_;
    }
}

template <typename AsyncStream>
void Subscription<AsyncStream>::write(tx_queue_t::element_type &queue) {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    std::stringstream out;
    while (queue.size()) {
        auto &item = queue.front();
        Protocol::serialize(out, std::get<0>(item), std::get<1>(item));
        queue.pop();
    }

    auto str = std::make_shared<std::string>(std::move(out.str()));
    auto str_ptr = str.get();
    BREDIS_LOG_DEBUG(">> " << str_ptr->c_str());
    asio::const_buffers_1 output_buf =
        asio::buffer(str_ptr->c_str(), str_ptr->size());

    asio::async_write(socket_, output_buf, [
        str_holder = str, this
    ](const sys::error_code &error_code, std::size_t bytes_transferred) {
        on_write(error_code, bytes_transferred);
    });
}

template <typename AsyncStream> void Subscription<AsyncStream>::read() {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    asio::async_read_until(socket_, rx_buff_, match_result,
                           [this](const sys::error_code &error_code,
                                  std::size_t bytes_transferred) {
                               on_read(error_code, bytes_transferred);
                           });
}

template <typename AsyncStream>
void Subscription<AsyncStream>::on_write(
    const boost::system::error_code &error_code,
    std::size_t bytes_transferred) {
    --tx_in_progress_;
    if (error_code) {
        BREDIS_LOG_DEBUG(error_code.message());
        callback_(error_code, {});
        return;
    }

    /* prioritize write over read, as in subscription they are rare */
    try_write();
    try_read();
}

template <typename AsyncStream>
void Subscription<AsyncStream>::on_read(
    const boost::system::error_code &error_code,
    std::size_t bytes_transferred) {
    if (error_code) {
        BREDIS_LOG_DEBUG(error_code.message());
        callback_(error_code, {});
        return;
    }

    // no sense to read/parse any longer as soon as
    // protocol error already met
    if (try_parse_rx()) {
        --rx_in_progress_;
    }

    std::unique_lock<std::mutex> lock(tx_queue_mutex_);
    auto has_pending_writers = !tx_queue_->empty();
    lock.unlock();

    if (has_pending_writers) {
        try_write();
    }
    try_read();
}

/* returns false in case of redis protocol error */
template <typename AsyncStream> bool Subscription<AsyncStream>::try_parse_rx() {
    bool try_parse;
    boost::system::error_code ec;
    redis_result_t redis_result;
    protocol_error_t *protocol_error = nullptr;
    do {
        try_parse = false;
        auto const_buff = rx_buff_.data();
        const char *char_ptr =
            boost::asio::buffer_cast<const char *>(const_buff);
        auto size = rx_buff_.size();
        string_t data(char_ptr, size);
        BREDIS_LOG_DEBUG("incoming data(" << size << ") : " << char_ptr);

        auto parse_result = Protocol::parse(data);
        if (parse_result.consumed == 0) {
            protocol_error = boost::get<protocol_error_t>(&parse_result.result);
            if (protocol_error == nullptr) {
                return true; /* no enough data */
            }
            BREDIS_LOG_DEBUG("protocol error: " << protocol_error->what);
            ec = Error::make_error_code(bredis_errors::protocol_error);
            try_parse = false;
        } else {
            try_parse = true;
            redis_result = boost::apply_visitor(some_result_visitor(),
                                                parse_result.result);
            rx_buff_.consume(parse_result.consumed);
        }

        callback_(ec, std::move(redis_result));
        if (protocol_error != nullptr) {
            return false;
        }
    } while (try_parse);
    return true;
}

} // namespace bredis
