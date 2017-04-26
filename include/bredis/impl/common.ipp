//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <boost/asio/read_until.hpp>
#include <boost/type_traits.hpp>
#include <iostream>
#include <sstream>
#include <boost/lexical_cast.hpp>

#ifdef BREDIS_DEBUG
#define BREDIS_LOG_DEBUG(msg)                                                  \
    { std::cout << __FILE__ << ":" << __LINE__ << " :: " << msg << std::endl; };
#else
#define BREDIS_LOG_DEBUG(msg) ;
#endif

namespace bredis {

typedef boost::asio::buffers_iterator<
    boost::asio::streambuf::const_buffers_type>
    asio_iterator;

class MatchResult {
  private:
    std::size_t matched_results_;
    std::size_t expected_count_;

  public:
    MatchResult(std::size_t expected_count)
        : expected_count_(expected_count), matched_results_(0) {}

    std::pair<asio_iterator, bool> operator()(asio_iterator begin,
                                              asio_iterator end) {
        const char *char_ptr = &*begin;
        auto size = std::distance(begin, end);

        BREDIS_LOG_DEBUG("will try to parse : " << char_ptr);

        auto parsing_complete = false;
        size_t consumed = 0;
        do {
            boost::string_ref data(char_ptr + consumed, size - consumed);
            auto parse_result = Protocol::parse(data);
            auto *parse_error = boost::get<protocol_error_t>(&parse_result);
            if (parse_error) {
                BREDIS_LOG_DEBUG("parse error : " << parse_error->what);
                consumed = 0;
                parsing_complete = true;
                break;
            }

            auto *no_enoght_data = boost::get<no_enoght_data_t>(&parse_result);
            if (no_enoght_data) {
                break;
            }

            auto &positive_result =
                boost::get<positive_parse_result_t>(parse_result);
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

struct result_stringizer: public boost::static_visitor<std::string> {
    std::string operator()(const int_result_t &value) const {
        return "[int] :: " +  boost::lexical_cast<std::string>(value);
    }

    std::string operator()(const string_holder_t &value) const {
        std::string str(value.str.cbegin(), value.str.cend());
        return "[string] :: " +  str;
    }

    std::string operator()(const error_holder_t &value) const {
        std::string str(value.str.cbegin(), value.str.cend());
        return "[error] :: " +  str;
    }

    std::string operator()(const nil_t &value) const {
        return "[nil_t]";
    }

    std::string operator()(const array_holder_t &value) const {
        std::string r = "{";
        for (const auto &v: value.elements) {
            r += boost::apply_visitor(result_stringizer(), v) ;
            r += ", ";
        }
        return r + "}";
    }
};

} // namespace bredis

namespace boost {
namespace asio {
template <>
struct is_match_condition<bredis::MatchResult> : public boost::true_type {};
}
}
