//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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
    using real_handler_t =
        typename asio::handler_type<WriteCallback, Signature>::type;

    std::ostream os(&tx_buff);
    auto string = boost::apply_visitor(command_serializer_visitor(), command);
    os.write(string.c_str(), string.size());

    real_handler_t handler(std::forward<WriteCallback>(write_callback));
    return async_write(stream_, tx_buff, handler);
}

template <typename NextLayer>
template <typename DynamicBuffer, typename ReadCallback>
BOOST_ASIO_INITFN_RESULT_TYPE(ReadCallback,
                              void(const boost::system::error_code,
                                   BREDIS_PARSE_RESULT(DynamicBuffer)))
Connection<NextLayer>::async_read(DynamicBuffer &rx_buff,
                                  ReadCallback &&read_callback,
                                  std::size_t replies_count) {

    namespace asio = boost::asio;
    namespace sys = boost::system;
    using boost::asio::async_read_until;
    using Iterator = typename to_iterator<DynamicBuffer>::iterator_t;
    using Signature =
        void(boost::system::error_code, BREDIS_PARSE_RESULT(DynamicBuffer));
    using real_handler_t =
        typename asio::handler_type<ReadCallback, Signature>::type;
    using result_t = ::boost::asio::async_result<real_handler_t>;

    real_handler_t real_handler(std::forward<ReadCallback>(read_callback));
    asio::async_result<real_handler_t> async_result(real_handler);

    async_read_op<NextLayer, DynamicBuffer, real_handler_t> async_op(
        std::move(real_handler), stream_, rx_buff, replies_count);

    async_read_until(stream_, rx_buff, MatchResult<Iterator>(replies_count),
                     std::move(async_op));
    return async_result.get();
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
BREDIS_PARSE_RESULT(DynamicBuffer)
Connection<NextLayer>::read(DynamicBuffer &rx_buff,
                            boost::system::error_code &ec) {
    namespace asio = boost::asio;
    using boost::asio::read_until;
    using Iterator = typename to_iterator<DynamicBuffer>::iterator_t;
    using result_t = BREDIS_PARSE_RESULT(DynamicBuffer);

    auto rx_bytes = read_until(stream_, rx_buff, MatchResult<Iterator>(1), ec);
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
BREDIS_PARSE_RESULT(DynamicBuffer)
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
