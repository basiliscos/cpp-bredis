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

struct base_result_visitor_t : public boost::static_visitor<std::size_t> {
    boost::system::error_code &error_code_;

    base_result_visitor_t(boost::system::error_code &error_code)
        : error_code_{error_code} {}

    std::size_t operator()(const not_enough_data_t &) const { std::abort(); }

    std::size_t operator()(const protocol_error_t &value) const {
        error_code_ = value.code;
        return 0;
    }
};

template <typename Iterator, typename Policy> struct result_visitor_t;

template <typename Iterator>
struct result_visitor_t<Iterator, parsing_policy::drop_result>
    : public base_result_visitor_t {
    using base_t = base_result_visitor_t;
    using policy_t = parsing_policy::drop_result;
    using positive_result_t = parse_result_mapper_t<Iterator, policy_t>;

    std::size_t replies_count;
    positive_result_t &result;
    size_t cumulative_consumption;
    size_t count;

    static positive_result_t construct() { return positive_result_t{0}; }

    result_visitor_t(boost::system::error_code &error_code,
                     std::size_t replies_count_, positive_result_t &result_)
        : base_t{error_code}, replies_count{replies_count_}, result{result_},
          cumulative_consumption{0}, count{0} {}

    void init() {
        // NO-OP;
    }

    using base_t::operator();

    size_t operator()(const positive_result_t &parse_result) {
        ++count;
        cumulative_consumption += parse_result.consumed;
        return count < replies_count ? parse_result.consumed : 0;
    }

    void complete_result() {
        result = positive_result_t{cumulative_consumption};
    }
};

template <typename Iterator>
struct result_visitor_t<Iterator, parsing_policy::keep_result>
    : public base_result_visitor_t {
    using base_t = base_result_visitor_t;
    using policy_t = parsing_policy::keep_result;
    using positive_result_t = parse_result_mapper_t<Iterator, policy_t>;

    std::size_t replies_count;
    positive_result_t &result;
    size_t cumulative_consumption;
    size_t count;
    markers::array_holder_t<Iterator> tmp_results;

    static positive_result_t construct() { return positive_result_t{}; }

    result_visitor_t(boost::system::error_code &error_code,
                     std::size_t replies_count_, positive_result_t &result_)
        : base_t{error_code}, replies_count{replies_count_}, result{result_},
          cumulative_consumption{0}, count{0} {}

    void init() { tmp_results.elements.reserve(replies_count); }

    using base_t::operator();

    size_t operator()(const positive_result_t &parse_result) {
        tmp_results.elements.emplace_back(std::move(parse_result.result));
        ++count;
        cumulative_consumption += parse_result.consumed;
        return count < replies_count ? parse_result.consumed : 0;
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

template <typename NextLayer, typename DynamicBuffer, typename Policy> struct async_read_op_impl {
    NextLayer &stream_;
    DynamicBuffer &rx_buff_;
    std::size_t replies_count_;
    enum class state_t { init, read, done } state_ = state_t::init;

    using Iterator = typename to_iterator<DynamicBuffer>::iterator_t;
    using ResultVisitor = result_visitor_t<Iterator, Policy>;
    using positive_result_t = parse_result_mapper_t<Iterator, Policy>;

    async_read_op_impl(NextLayer &stream, DynamicBuffer &rx_buff, std::size_t replies_count) :
        stream_(stream), rx_buff_(rx_buff), replies_count_(replies_count) {}

    positive_result_t op(boost::system::error_code &error_code,
                         std::size_t /*bytes_transferred*/) {

        auto result = ResultVisitor::construct();

        if (!error_code) {
            auto const_buff = rx_buff_.data();
            auto begin = Iterator::begin(const_buff);
            auto end = Iterator::end(const_buff);

            ResultVisitor visitor(error_code, replies_count_, result);
            visitor.init();

            std::size_t consumed{0};
            do {
                begin += consumed;
                auto parse_result =
                    Protocol::parse<Iterator, Policy>(begin, end);
                consumed = boost::apply_visitor(visitor, parse_result);
            } while (consumed);

            /* check again, as protocol error might be met */
            if (!error_code) {
                visitor.complete_result();
            }
        }
        return result;
    }

    template<typename Self>
    void operator()(Self& self, boost::system::error_code error_code = {}, std::size_t bytes_transferred = {}) {
        switch (state_) {
            case state_t::init:
                state_ = state_t::read;
                async_read_until(stream_, rx_buff_, MatchResult<Iterator>(replies_count_), std::move(self));
                break;
            case state_t::read:
            {
                state_ = state_t::done;
                auto result = op(error_code, bytes_transferred); // Do not inline! We are sequencing computations
                self.complete(error_code, result);
                break;
            }
            default:
                assert(false && "We are in unexpected state");
        }
    }
};

} // namespace bredis
