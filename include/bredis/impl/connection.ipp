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

namespace bredis {

template <typename Handler, typename Value> struct handler_frontend : Handler {

    Value v_;

    template <typename BackendHandler>
    handler_frontend(BackendHandler &&handler, Value &&v)
        : Handler(std::forward<BackendHandler>(handler)),
          v_(std::forward<Value>(v)) {}
};

template <typename NextLayer>
template <typename WriteCallback, typename DynamicBuffer>
BOOST_ASIO_INITFN_RESULT_TYPE(WriteCallback,
                              void(boost::system::error_code, std::size_t))
Connection<NextLayer>::async_write(DynamicBuffer &tx_buff,
                                   const command_wrapper_t &command,
                                   WriteCallback write_callback) {
    namespace asio = boost::asio;
    namespace sys = boost::system;
    using boost::asio::async_write;
    using real_handler_t =
        typename asio::handler_type<typename std::decay<WriteCallback>::type,
                                    void(boost::system::error_code,
                                         std::size_t)>::type;
    using frontend_handler_t = handler_frontend<real_handler_t, std::string>;

    std::ostream os(&tx_buff);
    auto string = boost::apply_visitor(command_serializer_visitor(), command);
    os.write(string.c_str(), string.size());
    tx_buff.commit(string.size());

    real_handler_t handler(std::forward<WriteCallback>(write_callback));
    return async_write(stream_, tx_buff, handler);
}

template <typename NextLayer>
template <typename ReadCallback, typename DynamicBuffer>
typename ::boost::asio::async_result<typename ::boost::asio::handler_type<
    ReadCallback, void(const boost::system::error_code &,
                       markers::redis_result_t<
                           typename to_iterator<DynamicBuffer>::iterator_t> &&,
                       std::size_t)>::type>::type
Connection<NextLayer>::async_read(DynamicBuffer &rx_buff,
                                  ReadCallback read_callback,
                                  std::size_t replies_count) {

    namespace asio = boost::asio;
    namespace sys = boost::system;
    using boost::asio::async_read_until;
    using Iterator = typename to_iterator<DynamicBuffer>::iterator_t;
    using real_handler_t =
        typename asio::handler_type<ReadCallback,
                                    void(const boost::system::error_code &,
                                         markers::redis_result_t<Iterator> &&,
                                         std::size_t)>::type;

    real_handler_t real_handler(std::forward<ReadCallback>(read_callback));
    asio::async_result<real_handler_t> result(real_handler);

    async_read_until(
        stream_, rx_buff, MatchResult<Iterator>(replies_count),
        [real_handler, &rx_buff, replies_count](
            const sys::error_code &error_code, std::size_t bytes_transferred) {
            markers::redis_result_t<Iterator> result;
            if (error_code) {
                real_handler(error_code, std::move(result), 0);
                return;
            }
            auto const_buff = rx_buff.data();
            auto begin = Iterator::begin(const_buff);
            auto end = Iterator::end(const_buff);

            markers::array_holder_t<Iterator> results;
            results.elements.reserve(replies_count);
            size_t cumulative_consumption = 0;
            boost::system::error_code ec;

            do {
                auto from = begin + cumulative_consumption;
                auto parse_result = Protocol::parse(from, end);
                auto *parse_error = boost::get<protocol_error_t>(&parse_result);
                if (parse_error) {
                    auto parse_error_code =
                        Error::make_error_code(bredis_errors::protocol_error);
                    real_handler(parse_error_code, std::move(result), 0);
                    return;
                }
                auto &positive_result =
                    boost::get<positive_parse_result_t<Iterator>>(parse_result);
                results.elements.emplace_back(positive_result.result);
                cumulative_consumption += positive_result.consumed;
            } while (results.elements.size() < replies_count);

            if (replies_count == 1) {
                real_handler(ec, std::move(results.elements[0]),
                             cumulative_consumption);
            } else {
                real_handler(ec, std::move(results), cumulative_consumption);
            }
        });

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
positive_parse_result_t<typename to_iterator<DynamicBuffer>::iterator_t>
Connection<NextLayer>::read(DynamicBuffer &rx_buff,
                            boost::system::error_code &ec) {
    namespace asio = boost::asio;
    using boost::asio::read_until;
    using Iterator = typename to_iterator<DynamicBuffer>::iterator_t;
    using result_t = positive_parse_result_t<
        typename to_iterator<DynamicBuffer>::iterator_t>;

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
        ec = Error::make_error_code(bredis_errors::protocol_error);
        return result_t{};
    }
    return boost::get<positive_parse_result_t<Iterator>>(parse_result);
}

template <typename NextLayer>
template <typename DynamicBuffer>
positive_parse_result_t<typename to_iterator<DynamicBuffer>::iterator_t>
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
