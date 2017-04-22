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
void AsyncConnection<AsyncStream>::push_command(const std::string &cmd,
                                                C &&contaier,
                                                command_callback_t callback) {
    args_container_t args{};
    std::copy(contaier.begin(), contaier.end(), std::back_inserter(args));

    auto callback_ptr =
        std::make_shared<command_callback_t>(std::move(callback));
    auto tuple = std::make_tuple(cmd, std::move(args), callback_ptr);

    std::unique_lock<std::mutex> lock(tx_queue_mutex_);
    tx_queue_->emplace(std::move(tuple));
    lock.unlock();

    try_write();
}

template <typename AsyncStream> void AsyncConnection<AsyncStream>::try_write() {
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

template <typename AsyncStream> void AsyncConnection<AsyncStream>::try_read() {
    BREDIS_LOG_DEBUG("try_read() rx_in_progress_ = " << rx_in_progress_);
    if (rx_in_progress_.fetch_add(1) == 0) {

        std::unique_lock<std::mutex> lock(rx_queue_mutex_);
        bool has_pending_readers = !rx_queue_.empty();
        lock.unlock();

        BREDIS_LOG_DEBUG(
            "try_read() has_pending_readers = " << has_pending_readers);
        if (has_pending_readers) {
            read();
        } else {
            --rx_in_progress_;
        }
    } else {
        --rx_in_progress_;
    }
}

template <typename AsyncStream>
void AsyncConnection<AsyncStream>::write(tx_queue_t::element_type &queue) {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    auto callbacks_queue = std::make_shared<callbacks_vector_t>();
    std::stringstream out;
    while (queue.size()) {
        auto &item = queue.front();
        Protocol::serialize(out, std::get<0>(item), std::get<1>(item));
        callbacks_queue->emplace_back(std::get<2>(item));
        queue.pop();
    }

    auto str = std::make_shared<std::string>(std::move(out.str()));
    auto str_ptr = str.get();
    BREDIS_LOG_DEBUG(">> " << str_ptr->c_str());
    asio::const_buffers_1 output_buf =
        asio::buffer(str_ptr->c_str(), str_ptr->size());

    asio::async_write(socket_, output_buf, [
        str_holder = str, callbacks_queue, this
    ](const sys::error_code &error_code, std::size_t bytes_transferred) {
        on_write(error_code, bytes_transferred, callbacks_queue);
    });
}

template <typename AsyncStream>
void AsyncConnection<AsyncStream>::on_write(
    const boost::system::error_code &error_code, std::size_t bytes_transferred,
    std::shared_ptr<callbacks_vector_t> callbacks) {
    --tx_in_progress_;
    if (error_code) {
        BREDIS_LOG_DEBUG(error_code.message());
        // TODO: more precisely determine for which error error happen?
        auto &callback_ptr = callbacks->at(0);
        (*callback_ptr)(error_code, {});
        return;
    }
    std::unique_lock<std::mutex> lock(rx_queue_mutex_);
    for (auto &callback_ptr : *callbacks) {
        rx_queue_.emplace(callback_ptr);
    }
    BREDIS_LOG_DEBUG("rx_queue_ size: " << rx_queue_.size());
    lock.unlock();

    try_read();
    try_write();
}

template <typename AsyncStream> void AsyncConnection<AsyncStream>::read() {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    BREDIS_LOG_DEBUG("async_read_until, rx_in_progress_ = " << rx_in_progress_);

    asio::async_read_until(socket_, rx_buff_, match_result,
                           [this](const sys::error_code &error_code,
                                  std::size_t bytes_transferred) {
                               on_read(error_code, bytes_transferred);
                           });
}

template <typename AsyncStream>
void AsyncConnection<AsyncStream>::on_read(
    const boost::system::error_code &error_code,
    std::size_t bytes_transferred) {
    if (error_code) {
        BREDIS_LOG_DEBUG(error_code.message());
        // TODO: more precisely determine for which error error happen?
        std::unique_lock<std::mutex> lock(rx_queue_mutex_);
        auto callback_ptr = rx_queue_.front();
        rx_queue_.pop();
        lock.unlock();
        (*callback_ptr)(error_code, {});
        return;
    }

    // no sense to read/parse any longer as soon as
    // protocol error already met
    if (try_parse_rx()) {
        --rx_in_progress_;
    }

    BREDIS_LOG_DEBUG(
        "async_read_until, finishing, rx_in_progress_ = " << rx_in_progress_);
    try_read();
}

/* returns false in case of redis protocol error */
template <typename AsyncStream>
bool AsyncConnection<AsyncStream>::try_parse_rx() {
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

        std::unique_lock<std::mutex> lock(rx_queue_mutex_);
        auto callback_ptr = rx_queue_.front();
        rx_queue_.pop();
        BREDIS_LOG_DEBUG("rx queue size left: " << rx_queue_.size());
        lock.unlock();
        (*callback_ptr)(ec, std::move(redis_result));

        if (protocol_error != nullptr) {
            return false;
        }
    } while (try_parse);
    return true;
}

} // namespace bredis
