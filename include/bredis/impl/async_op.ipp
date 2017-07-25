//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <memory>
#include <utility>

#include <boost/asio.hpp>

namespace bredis {

template <typename NextLayer, typename DynamicBuffer, typename ReadCallback>
class async_read_op {
    NextLayer &stream_;
    DynamicBuffer &rx_buff_;
    ReadCallback callback_;
    std::size_t replies_count_;

  public:
    async_read_op(async_read_op &&) = default;
    async_read_op(const async_read_op &) = default;

    template <class DeducedHandler>
    async_read_op(DeducedHandler &&deduced_handler, NextLayer &stream,
                  DynamicBuffer &rx_buff, std::size_t replies_count)
        : stream_(stream), rx_buff_(rx_buff), replies_count_(replies_count),
          callback_(std::forward<ReadCallback>(deduced_handler)) {}

    void operator()(boost::system::error_code, std::size_t bytes_transferred);

    friend bool asio_handler_is_continuation(async_read_op *op) {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(op->callback_));
    }

    friend void *asio_handler_allocate(std::size_t size, async_read_op *op) {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(size, std::addressof(op->callback_));
    }

    friend void asio_handler_deallocate(void *p, std::size_t size,
                                        async_read_op *op) {
        using boost::asio::asio_handler_deallocate;
        return asio_handler_deallocate(p, size, std::addressof(op->callback_));
    }

    template <class Function>
    friend void asio_handler_invoke(Function &&f, async_read_op *op) {
        using boost::asio::asio_handler_invoke;
        return asio_handler_invoke(f, std::addressof(op->callback_));
    }
};

template <typename NextLayer, typename DynamicBuffer, typename ReadCallback>
void async_read_op<NextLayer, DynamicBuffer, ReadCallback>::
operator()(boost::system::error_code error_code,
           std::size_t bytes_transferred) {
    using Iterator = typename to_iterator<DynamicBuffer>::iterator_t;
    using positive_result_t =
        parse_result_mapper_t<Iterator, parsing_policy::keep_result>;

    positive_result_t result;

    if (!error_code) {
        auto const_buff = rx_buff_.data();
        auto begin = Iterator::begin(const_buff);
        auto end = Iterator::end(const_buff);

        markers::array_holder_t<Iterator> results;
        results.elements.reserve(replies_count_);
        size_t cumulative_consumption = 0;

        do {
            auto parse_result = Protocol::parse(begin, end);
            auto *parse_error = boost::get<protocol_error_t>(&parse_result);
            if (parse_error) {
                error_code = parse_error->code;
                break;
            } else {
                auto &positive_result =
                    boost::get<positive_result_t>(parse_result);
                results.elements.emplace_back(positive_result.result);
                begin += positive_result.consumed;
                cumulative_consumption += positive_result.consumed;
            }
        } while (results.elements.size() < replies_count_);

        /* check again, as protocol error might be met */
        if (!error_code) {
            if (replies_count_ == 1) {
                result = positive_result_t{std::move(results.elements[0]),
                                           cumulative_consumption};
            } else {
                result = positive_result_t{std::move(results),
                                           cumulative_consumption};
            }
        }
    }

    callback_(error_code, std::move(result));
}

} // namespace bredis
