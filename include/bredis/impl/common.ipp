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
#include <sstream>

#ifdef BREDIS_DEBUG
#define BREDIS_LOG_DEBUG(msg)                                                  \
    { std::cout << __FILE__ << ":" << __LINE__ << " :: " << msg << std::endl; };
#else
#define BREDIS_LOG_DEBUG(msg) ;
#endif

namespace bredis {

template <typename Iterator> class MatchResult {
  private:
    std::size_t matched_results_;
    std::size_t expected_count_;

  public:
    MatchResult(std::size_t expected_count)
        : expected_count_(expected_count), matched_results_(0) {}

    std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) {
        auto parsing_complete = false;
        size_t consumed = 0;
        do {
            auto from = begin + consumed;
            auto parse_result = Protocol::parse(from, end);
            auto *parse_error = boost::get<protocol_error_t>(&parse_result);
            if (parse_error) {
                // BREDIS_LOG_DEBUG("parse error : " << parse_error->what);
                consumed = 0;
                parsing_complete = true;
                break;
            }

            auto *no_enoght_data = boost::get<no_enogh_data_t>(&parse_result);
            if (no_enoght_data) {
                break;
            }

            auto &positive_result =
                boost::get<positive_parse_result_t<Iterator>>(parse_result);
            ++matched_results_;
            consumed += positive_result.consumed;
            parsing_complete = (matched_results_ == expected_count_);
        } while (!parsing_complete);
        return std::make_pair(begin + consumed, parsing_complete);
    }
};

class command_serializer_visitor : public boost::static_visitor<std::string> {
  public:
    std::string operator()(const single_command_t &value) const {
        std::stringstream out;
        Protocol::serialize(out, value);
        return out.str();
    }

    std::string operator()(const command_container_t &value) const {
        std::stringstream out;
        for (const auto &cmd : value) {
            Protocol::serialize(out, cmd);
        }
        return out.str();
    }
};

} // namespace bredis

namespace boost {
namespace asio {

template <typename Iterator>
struct is_match_condition<bredis::MatchResult<Iterator>>
    : public boost::true_type {};
}
}
