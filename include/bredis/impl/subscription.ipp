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

template <typename AsyncStream, typename NotificationCallback>
template <typename WriteCallback>
void Subscription<AsyncStream, NotificationCallback>::async_write(
    const command_wrapper_t &command, WriteCallback write_callback) {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    auto str = std::make_shared<std::string>(
        boost::apply_visitor(command_serializer_visitor(), command));
    auto str_ptr = str.get();
    BREDIS_LOG_DEBUG("async_write >> " << str_ptr->c_str());
    asio::const_buffers_1 output_buf =
        asio::buffer(str_ptr->c_str(), str_ptr->size());

    asio::async_write(socket_, output_buf,
                      [str, write_callback](const sys::error_code &error_code,
                                            std::size_t bytes_transferred) {
                          write_callback(error_code);
                      });
}

template <typename AsyncStream, typename NotificationCallback>
void Subscription<AsyncStream, NotificationCallback>::async_read() {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    BREDIS_LOG_DEBUG("async_read");

    asio::async_read_until(
        socket_, rx_buff_, MatchResult(1),
        [this](const sys::error_code &error_code,
               std::size_t bytes_transferred) {
            if (error_code) {
                callback_(error_code, {});
                return;
            }

            auto const_buff = rx_buff_.data();
            const char *char_ptr =
                boost::asio::buffer_cast<const char *>(const_buff);
            auto size = rx_buff_.size();
            string_t data(char_ptr, size);
            BREDIS_LOG_DEBUG("incoming data("
                             << size << ") : " << char_ptr
                             << ", tx bytes: " << bytes_transferred);

            auto parse_result = Protocol::parse(data);

            auto *parse_error = boost::get<protocol_error_t>(&parse_result);
            if (parse_error) {
                BREDIS_LOG_DEBUG("protocol error: " << protocol_error->what);
                auto parse_error_code =
                    Error::make_error_code(bredis_errors::protocol_error);
                callback_(parse_error_code, {});
                return;
            }

            boost::system::error_code ec;
            auto &positive_result =
                boost::get<positive_parse_result_t>(parse_result);
            callback_(ec, std::move(positive_result.result));
            rx_buff_.consume(positive_result.consumed);
            async_read();
        });
}

} // namespace bredis
