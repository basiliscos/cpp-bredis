//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include "common.ipp"

namespace bredis {

template <typename AsyncStream>
template <typename C>
redis_result_t
SyncConnection<AsyncStream>::command(const std::string &cmd, C &&container,
                                     boost::asio::streambuf &rx_buff) {
    namespace asio = boost::asio;

    std::stringstream out;
    Protocol::serialize(out, cmd, container);
    std::string out_str(out.str());

    asio::const_buffers_1 output_buf =
        asio::buffer(out_str.c_str(), out_str.size());

    asio::write(socket_, output_buf);
    auto rx_bytes = asio::read_until(socket_, rx_buff, match_result);

    const char *char_ptr =
        boost::asio::buffer_cast<const char *>(rx_buff.data());
    auto size = rx_buff.size();
    string_t data(char_ptr, size);
    BREDIS_LOG_DEBUG("incoming data(" << size << ") : " << char_ptr);

    auto parse_result = Protocol::parse(data);
    if (parse_result.consumed == 0) {
        auto protocol_error = boost::get<protocol_error_t>(parse_result.result);
        BREDIS_LOG_DEBUG("protocol error: " << protocol_error.what);
        throw Error::make_error_code(bredis_errors::protocol_error);
    }
    rx_buff.consume(parse_result.consumed);
    return boost::apply_visitor(some_result_visitor(), parse_result.result);
}

} // namespace bredis
