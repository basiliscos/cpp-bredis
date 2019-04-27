//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <boost/asio/read_until.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/type_traits.hpp>
#include <iostream>
#include <locale>
#include <sstream>

#ifdef BREDIS_DEBUG
#define BREDIS_LOG_DEBUG(msg)                                                  \
    { std::cout << __FILE__ << ":" << __LINE__ << " :: " << msg << std::endl; };
#else
#define BREDIS_LOG_DEBUG(msg) ;
#endif

namespace bredis {

template <typename Iterator, typename Policy>
struct consumed_parse : public boost::static_visitor<int> {
    int
    operator()(const positive_parse_result_t<Iterator, Policy> &value) const {
        return static_cast<int>(value.consumed);
    }

    int operator()(const not_enough_data_t & /*value*/) const { return 0; }

    int operator()(const protocol_error_t & /*value*/) const { return -1; }
};

template <typename Iterator> class MatchResult {
  private:
    std::size_t expected_count_;
    std::size_t matched_results_;

  public:
    MatchResult(std::size_t expected_count)
        : expected_count_(expected_count), matched_results_(0) {}

    std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) {
        using Policy = bredis::parsing_policy::drop_result;

        auto parsing_complete = false;
        size_t consumed = 0;
        auto parse_from = begin;
        do {
            auto from = parse_from;
            auto parse_result = Protocol::parse<Iterator, Policy>(from, end);
            auto consumable = boost::apply_visitor(
                consumed_parse<Iterator, Policy>(), parse_result);
            if (consumable == -1) {
                // parse error
                consumed = 0;
                parsing_complete = true;
                break;
            }
            if (consumable == 0) {
                // no enough data
                break;
            }
            ++matched_results_;
            auto iteration_consumed = static_cast<std::size_t>(consumable);
            consumed += iteration_consumed;
            parse_from += iteration_consumed;
            parsing_complete = (matched_results_ == expected_count_);
        } while (!parsing_complete);
        return std::make_pair(begin + consumed, parsing_complete);
    }
};

template <typename DynamicBuffer>
class command_serializer_visitor : public boost::static_visitor<void> {
  private:
    DynamicBuffer &buff_;

  public:
    command_serializer_visitor(DynamicBuffer &buff) : buff_{buff} {}
    void operator()(const single_command_t &value) const {
        Protocol::serialize(buff_, value);
    }

    void operator()(const command_container_t &value) const {
        for (const auto &cmd : value) {
            Protocol::serialize(buff_, cmd);
        }
    }
};

} // namespace bredis

namespace boost {
namespace asio {

template <typename Iterator>
struct is_match_condition<bredis::MatchResult<Iterator>>
    : public boost::true_type {};
} // namespace asio
} // namespace boost
