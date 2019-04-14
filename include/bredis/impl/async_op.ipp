//
//
// Copyright (c) 2017, 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail
// dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include "../Protocol.hpp"
#include "../Result.hpp"
#include <memory>
#include <utility>

#include <boost/asio.hpp>
#include <boost/variant.hpp>

namespace bredis {

template <typename Iterator, typename Policy> struct result_handler_t;

template <typename Iterator>
struct result_handler_t<Iterator, parsing_policy::drop_result> {
    using policy_t = parsing_policy::drop_result;
    using positive_result_t = parse_result_mapper_t<Iterator, policy_t>;

    std::size_t replies_count;
    size_t cumulative_consumption;
    size_t count;
    positive_result_t result;

    result_handler_t(std::size_t replies_count_)
        : replies_count{replies_count_},
          cumulative_consumption{0}, count{0}, result{0} {}

    void init() {
        // NO-OP;
    }

    bool on_result(positive_result_t &&parse_result) {
        ++count;
        cumulative_consumption += parse_result.consumed;
        return count < replies_count;
    }

    void complete_result() {
        result = positive_result_t{cumulative_consumption};
    }
};

template <typename Iterator>
struct result_handler_t<Iterator, parsing_policy::keep_result> {
    using policy_t = parsing_policy::keep_result;
    using positive_result_t = parse_result_mapper_t<Iterator, policy_t>;

    std::size_t replies_count;
    size_t cumulative_consumption;
    size_t count;
    positive_result_t result;
    markers::array_holder_t<Iterator> tmp_results;

    result_handler_t(std::size_t replies_count_)
        : replies_count{replies_count_}, cumulative_consumption{0}, count{0} {}

    void init() { tmp_results.elements.reserve(replies_count); }

    bool on_result(positive_result_t &&parse_result) {
        tmp_results.elements.emplace_back(std::move(parse_result.result));
        ++count;
        cumulative_consumption += parse_result.consumed;
        return count < replies_count;
    }

    void complete_result() {
        if (replies_count == 1) {
            result = positive_result_t{std::move(tmp_results.elements[0]),
                                       cumulative_consumption};
        } else {
            result = positive_result_t{std::move(tmp_results),
                                       cumulative_consumption};
        }
    }
};

template <typename DynamicBuffer, typename Policy> struct async_read_op_impl {
    DynamicBuffer &rx_buff_;
    std::size_t replies_count_;

    async_read_op_impl(DynamicBuffer &rx_buff, std::size_t replies_count)
        : rx_buff_{rx_buff}, replies_count_{replies_count} {}
    using Iterator = typename to_iterator<DynamicBuffer>::iterator_t;
    using ResultHandler = result_handler_t<Iterator, Policy>;
    using positive_result_t = parse_result_mapper_t<Iterator, Policy>;

    positive_result_t op(boost::system::error_code &error_code,
                         std::size_t /*bytes_transferred*/) {

        ResultHandler result_handler(replies_count_);

        if (!error_code) {
            auto const_buff = rx_buff_.data();
            auto begin = Iterator::begin(const_buff);
            auto end = Iterator::end(const_buff);

            result_handler.init();

            bool continue_parsing;
            do {
                using boost::get;
                auto parse_result =
                    Protocol::parse<Iterator, Policy>(begin, end);
                auto *parse_error = boost::get<protocol_error_t>(&parse_result);
                if (parse_error) {
                    error_code = parse_error->code;
                    continue_parsing = false;
                } else {
                    auto &positive_result =
                        get<positive_result_t>(parse_result);
                    begin += positive_result.consumed;
                    continue_parsing =
                        result_handler.on_result(std::move(positive_result));
                }
            } while (continue_parsing);

            /* check again, as protocol error might be met */
            if (!error_code) {
                result_handler.complete_result();
            }
        }
        return result_handler.result;
    }
};

template <typename NextLayer, typename DynamicBuffer, typename ReadCallback,
          typename Policy>
class async_read_op {
    NextLayer &stream_;
    DynamicBuffer &rx_buff_;
    std::size_t replies_count_;
    ReadCallback callback_;

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

template <typename NextLayer, typename DynamicBuffer, typename ReadCallback,
          typename Policy>
void async_read_op<NextLayer, DynamicBuffer, ReadCallback, Policy>::
operator()(boost::system::error_code error_code,
           std::size_t bytes_transferred) {
    using op_impl = async_read_op_impl<DynamicBuffer, Policy>;
    callback_(
        error_code,
        op_impl(rx_buff_, replies_count_).op(error_code, bytes_transferred));
}

} // namespace bredis
