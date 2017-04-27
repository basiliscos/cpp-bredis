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

template <typename NextLayer>
template <typename WriteCallback>
void Connection<NextLayer>::async_write(const command_wrapper_t &command,
                                        WriteCallback write_callback) {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    auto str = std::make_shared<std::string>(
        boost::apply_visitor(command_serializer_visitor(), command));
    auto str_ptr = str.get();
    BREDIS_LOG_DEBUG("async_write >> " << str_ptr->c_str());
    asio::const_buffers_1 output_buf =
        asio::buffer(str_ptr->c_str(), str_ptr->size());

    asio::async_write(stream_, output_buf,
                      [str, write_callback](const sys::error_code &error_code,
                                            std::size_t bytes_transferred) {
                          write_callback(error_code);
                      });
}

template <typename NextLayer>
template <typename ReadCallback, typename DynamicBuffer>
void Connection<NextLayer>::async_read(DynamicBuffer &rx_buff,
                                       ReadCallback read_callback,
                                       std::size_t replies_count) {

    namespace asio = boost::asio;
    namespace sys = boost::system;

    BREDIS_LOG_DEBUG("async_read");

    asio::async_read_until(
        stream_, rx_buff, MatchResult(replies_count),
        [read_callback, &rx_buff, replies_count](
            const sys::error_code &error_code, std::size_t bytes_transferred) {
            if (error_code) {
                read_callback(error_code, {}, 0);
                return;
            }

            auto const_buff = rx_buff.data();
            const char *char_ptr =
                boost::asio::buffer_cast<const char *>(const_buff);
            auto size = rx_buff.size();
            BREDIS_LOG_DEBUG("incoming data("
                             << size << ") : " << char_ptr
                             << ", tx bytes: " << bytes_transferred);

            array_holder_t results;
            results.elements.reserve(replies_count);
            size_t cumulative_consumption = 0;
            boost::system::error_code ec;

            do {
                string_t data(char_ptr + cumulative_consumption,
                              size - cumulative_consumption);
                BREDIS_LOG_DEBUG("trying to get response # "
                                 << results.elements.size());
                BREDIS_LOG_DEBUG("trying to get response from " << data);
                auto parse_result = Protocol::parse(data);
                auto *parse_error = boost::get<protocol_error_t>(&parse_result);
                if (parse_error) {
                    /* might happen only in case of protocol error */
                    BREDIS_LOG_DEBUG("protocol error: " << parse_error->what);
                    auto parse_error_code =
                        Error::make_error_code(bredis_errors::protocol_error);
                    read_callback(parse_error_code, {}, 0);
                    return;
                }
                auto &positive_result =
                    boost::get<positive_parse_result_t>(parse_result);
                results.elements.emplace_back(positive_result.result);
                cumulative_consumption += positive_result.consumed;
            } while (results.elements.size() < replies_count);

            if (replies_count == 1) {
                read_callback(ec, std::move(results.elements[0]),
                              cumulative_consumption);
            } else {
                read_callback(ec, std::move(results), cumulative_consumption);
            }
        });
}

template <typename NextLayer>
void Connection<NextLayer>::write(const command_wrapper_t &command) {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    auto str = boost::apply_visitor(command_serializer_visitor(), command);
    BREDIS_LOG_DEBUG("async_write >> " << str);
    asio::const_buffers_1 output_buf = asio::buffer(str.c_str(), str.size());
    asio::write(stream_, output_buf);
}

template <typename NextLayer>
void Connection<NextLayer>::write(const command_wrapper_t &command,
                                  boost::system::error_code &ec) {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    auto str = boost::apply_visitor(command_serializer_visitor(), command);
    BREDIS_LOG_DEBUG("async_write >> " << str);
    asio::const_buffers_1 output_buf = asio::buffer(str.c_str(), str.size());
    asio::write(stream_, output_buf, ec);
}

template <typename NextLayer>
template <typename DynamicBuffer>
positive_parse_result_t Connection<NextLayer>::read(DynamicBuffer &rx_buff) {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    auto rx_bytes = asio::read_until(stream_, rx_buff, MatchResult(1));

    const char *char_ptr =
        boost::asio::buffer_cast<const char *>(rx_buff.data());
    auto size = rx_buff.size();
    string_t data(char_ptr, size);
    BREDIS_LOG_DEBUG("incoming data(" << size << ") : " << char_ptr);

    auto parse_result = Protocol::parse(data);
    auto *parse_error = boost::get<protocol_error_t>(&parse_result);
    if (parse_error) {
        BREDIS_LOG_DEBUG("protocol error: " << parse_error->what);
        throw Error::make_error_code(bredis_errors::protocol_error);
    }
    return boost::get<positive_parse_result_t>(parse_result);
}

template <typename NextLayer>
template <typename DynamicBuffer>
positive_parse_result_t
Connection<NextLayer>::read(DynamicBuffer &rx_buff,
                            boost::system::error_code &ec) {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    auto rx_bytes = asio::read_until(stream_, rx_buff, MatchResult(1), ec);

    const char *char_ptr =
        boost::asio::buffer_cast<const char *>(rx_buff.data());
    auto size = rx_buff.size();
    string_t data(char_ptr, size);
    BREDIS_LOG_DEBUG("incoming data(" << size << ") : " << char_ptr);

    auto parse_result = Protocol::parse(data);
    auto *parse_error = boost::get<protocol_error_t>(&parse_result);
    if (parse_error) {
        BREDIS_LOG_DEBUG("protocol error: " << parse_error->what);
        ec = Error::make_error_code(bredis_errors::protocol_error);
        return positive_parse_result_t{{}, 0};
    }
    return boost::get<positive_parse_result_t>(parse_result);
}

} // namespace bredis
