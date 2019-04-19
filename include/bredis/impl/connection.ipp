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
#include <cassert>
#include <ostream>
#include <type_traits>

#include "async_op.ipp"

namespace bredis {

template <typename NextLayer>
template <typename DynamicBuffer, typename WriteCallback>
BOOST_ASIO_INITFN_RESULT_TYPE(WriteCallback,
                              void(boost::system::error_code, std::size_t))
Connection<NextLayer>::async_write(DynamicBuffer &tx_buff,
                                   const command_wrapper_t &command,
                                   WriteCallback &&write_callback) {
    namespace asio = boost::asio;
    namespace sys = boost::system;
    using boost::asio::async_write;
    using Signature = void(boost::system::error_code, std::size_t);
    using AsyncResult = asio::async_result<std::decay_t<WriteCallback>,Signature>;
    using CompletionHandler = typename AsyncResult::completion_handler_type;

    std::ostream os(&tx_buff);
    auto string = boost::apply_visitor(command_serializer_visitor(), command);
    os.write(string.c_str(), string.size());
    
    CompletionHandler handler(std::forward<WriteCallback>(write_callback));
    AsyncResult result(handler);
    async_write(stream_, tx_buff, handler);
    return result.get();
}

template <typename NextLayer>
template <typename DynamicBuffer, typename ReadCallback, typename Policy>
BOOST_ASIO_INITFN_RESULT_TYPE(ReadCallback,
                              void(const boost::system::error_code,
                                   BREDIS_PARSE_RESULT(DynamicBuffer, Policy)))
Connection<NextLayer>::async_read(DynamicBuffer &rx_buff,
                                  ReadCallback &&read_callback,
                                  std::size_t replies_count, Policy) {

    namespace asio = boost::asio;
    namespace sys = boost::system;
    using boost::asio::async_read_until;
    using Iterator = typename to_iterator<DynamicBuffer>::iterator_t;
    using ParseResult = BREDIS_PARSE_RESULT(DynamicBuffer, Policy);
    using Signature = void(boost::system::error_code, ParseResult);
    using AsyncResult = asio::async_result<std::decay_t<ReadCallback>,Signature>;
    using CompletionHandler = typename AsyncResult::completion_handler_type;
		
    CompletionHandler handler(std::forward<ReadCallback>(read_callback));
    AsyncResult result(handler);
		
    async_read_op<NextLayer, DynamicBuffer, CompletionHandler, Policy> async_op(
        handler, stream_, rx_buff, replies_count);

    async_read_until(stream_, rx_buff, MatchResult<Iterator>(replies_count),
                     std::move(async_op));
    return result.get();
}

template <typename NextLayer>
void Connection<NextLayer>::write(const command_wrapper_t &command,
                                  boost::system::error_code &ec) {
    namespace asio = boost::asio;
    auto str = boost::apply_visitor(command_serializer_visitor(), command);
    auto const output_buf = asio::buffer(str.c_str(), str.size());
    asio::write(stream_, output_buf, ec);
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
