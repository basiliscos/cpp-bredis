//
//
// Copyright (c) 2017, 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail
// dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include "common.ipp"
#include <algorithm>
#include <boost/type_traits/decay.hpp>
#include <cassert>
#include <ostream>

#include "async_op.ipp"

namespace bredis {

template <typename NextLayer>
template <typename DynamicBuffer, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
                              void(boost::system::error_code, std::size_t))
Connection<NextLayer>::async_write(DynamicBuffer &tx_buff,
                                   const command_wrapper_t &command,
                                   CompletionToken &&write_callback) {
    namespace asio = boost::asio;
    namespace sys = boost::system;

    using boost::asio::async_write;
    using serializer_t = command_serializer_visitor<DynamicBuffer>;

    boost::apply_visitor(serializer_t(tx_buff), command);
    return async_write(stream_, tx_buff, std::forward<CompletionToken>(write_callback));
}

template <typename NextLayer>
template <typename DynamicBuffer, typename CompletionToken, typename Policy>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
                              void(boost::system::error_code,
                                   BREDIS_PARSE_RESULT(DynamicBuffer, Policy)))
Connection<NextLayer>::async_read(DynamicBuffer &rx_buff,
                                  CompletionToken &&completion_token,
                                  std::size_t replies_count, Policy) {
    using boost::asio::async_read_until;
    using ParseResult = BREDIS_PARSE_RESULT(DynamicBuffer, Policy);
    using Signature = void(boost::system::error_code, ParseResult);

    return boost::asio::async_compose<CompletionToken, Signature>(
        async_read_op_impl<NextLayer, DynamicBuffer, Policy>{stream_, rx_buff, replies_count},
        completion_token, stream_);
}

template <typename NextLayer>
void Connection<NextLayer>::write(const command_wrapper_t &command,
                                  boost::system::error_code &ec) {
    namespace asio = boost::asio;
    asio::streambuf tx_buff;
    using serializer_t = command_serializer_visitor<asio::streambuf>;

    boost::apply_visitor(serializer_t(tx_buff), command);
    asio::write(stream_, tx_buff, ec);
}

template <typename NextLayer>
void Connection<NextLayer>::write(const command_wrapper_t &command) {
    boost::system::error_code ec;
    this->write(command, ec);
    if (ec) {
        throw boost::system::system_error{ec};
    }
}

template <typename NextLayer>
template <typename DynamicBuffer>
BREDIS_PARSE_RESULT(DynamicBuffer, bredis::parsing_policy::keep_result)
Connection<NextLayer>::read(DynamicBuffer &rx_buff,
                            boost::system::error_code &ec) {
    namespace asio = boost::asio;
    using boost::asio::read_until;

    using Iterator = typename to_iterator<DynamicBuffer>::iterator_t;
    using Policy = bredis::parsing_policy::keep_result;
    using result_t = BREDIS_PARSE_RESULT(DynamicBuffer, Policy);

    /*auto rx_bytes =*/read_until(stream_, rx_buff, MatchResult<Iterator>(1),
                                  ec);
    if (ec) {
        return result_t{{}, 0};
    }

    auto const_buff = rx_buff.data();
    auto begin = Iterator::begin(const_buff);
    auto end = Iterator::end(const_buff);

    auto parse_result = Protocol::parse(begin, end);

    auto *parse_error = boost::get<protocol_error_t>(&parse_result);
    if (parse_error) {
        ec = parse_error->code;
        return result_t{};
    }
    return boost::get<result_t>(parse_result);
}

template <typename NextLayer>
template <typename DynamicBuffer>
BREDIS_PARSE_RESULT(DynamicBuffer, bredis::parsing_policy::keep_result)
Connection<NextLayer>::read(DynamicBuffer &rx_buff) {
    namespace asio = boost::asio;

    boost::system::error_code ec;
    auto result = this->read(rx_buff, ec);
    if (ec) {
        throw boost::system::system_error{ec};
    }
    return result;
}

} // namespace bredis
