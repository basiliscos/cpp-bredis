//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <algorithm>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/utility/string_ref.hpp>

namespace bredis {

static inline const boost::string_ref &get_terminator() {
    static boost::string_ref terminator("\r\n");
    return terminator;
}

template <typename Iterator, typename Policy>
static optional_parse_result_t<Iterator, Policy> raw_parse(Iterator &from,
                                                           Iterator &to);

namespace extractor_tags {
struct e_string;
struct e_error;
struct e_int;
struct e_bulk_string;
struct e_array;
}; // extractors

template <typename Iterator, typename Policy> struct ExtractorHelper {
    using optional_wrapper_t = optional_parse_result_t<Iterator, Policy>;
    using positive_wrapper_t = positive_parse_result_t<Iterator, Policy>;

    static auto extract_string(size_t consumed, Iterator from, Iterator to)
        -> optional_wrapper_t {
        return optional_wrapper_t{
            positive_wrapper_t{markers::redis_result_t<Iterator>{
                                   markers::string_t<Iterator>{from, to}},
                               consumed}};
    }

    static auto extract_error(parse_result_mapper_t<Iterator, Policy> *string)
        -> optional_wrapper_t {
        auto &string_result =
            boost::get<markers::string_t<Iterator>>(string->result);
        return optional_wrapper_t{
            positive_wrapper_t{markers::redis_result_t<Iterator>{
                                   markers::error_t<Iterator>{string_result}},
                               string->consumed}};
    }

    static auto extract_int(parse_result_mapper_t<Iterator, Policy> *string)
        -> optional_wrapper_t {
        auto &string_result =
            boost::get<markers::string_t<Iterator>>(string->result);
        return optional_wrapper_t{
            positive_wrapper_t{markers::redis_result_t<Iterator>{
                                   markers::int_t<Iterator>{string_result}},
                               string->consumed}};
    }

    static auto extract_nil(size_t consumed,
                            markers::string_t<Iterator> &string)
        -> optional_wrapper_t {
        return optional_wrapper_t{positive_wrapper_t{
            markers::redis_result_t<Iterator>{markers::nil_t<Iterator>{string}},
            consumed}};
    }

    static auto extract_bulk_string(size_t already_consumed, size_t shift,
                                    int count, Iterator from, Iterator to)
        -> optional_wrapper_t {
        auto head = from + shift;
        size_t left = std::distance(head, to);
        size_t ucount = static_cast<size_t>(count);
        const auto &terminator = get_terminator();
        auto terminator_size = terminator.size();
        if (left < ucount + terminator_size) {
            return not_enough_data_t{};
        }
        auto tail = head + ucount;
        auto tail_end = tail + terminator_size;

        auto from_t(get_terminator().cbegin()), to_t(get_terminator().cend());
        bool found_terminator = std::equal(tail, tail_end, from_t, to_t);
        if (!found_terminator) {
            throw std::runtime_error("Terminator not found for bulk string");
        }
        size_t consumed = shift + count + terminator_size + already_consumed;

        return optional_wrapper_t{
            positive_wrapper_t{markers::redis_result_t<Iterator>{
                                   markers::string_t<Iterator>{head, tail}},
                               consumed}};
    }

    static auto extract_array(size_t already_consumed, size_t shift, int count,
                              Iterator from, Iterator to)
        -> optional_wrapper_t {

        using positive_wrapper_t = positive_parse_result_t<Iterator, Policy>;

        auto result = markers::array_holder_t<Iterator>{};
        result.elements.reserve(count);
        for (auto i = 0; i < count; ++i) {
            Iterator left = from + shift;
            auto optional_parse_result =
                raw_parse<Iterator, parsing_policy::keep_result>(left, to);
            auto *no_enogh_data =
                boost::get<not_enough_data_t>(&optional_parse_result);
            if (no_enogh_data) {
                return *no_enogh_data;
            }
            auto &parsed_data =
                boost::get<positive_wrapper_t>(optional_parse_result);
            result.elements.emplace_back(parsed_data.result);
            shift += parsed_data.consumed;
        }
        return optional_wrapper_t{
            positive_wrapper_t{markers::redis_result_t<Iterator>{result},
                               shift + already_consumed}};
    }
};

template <typename Iterator>
struct ExtractorHelper<Iterator, parsing_policy::drop_result> {
    using Policy = parsing_policy::drop_result;
    using optional_wrapper_t = optional_parse_result_t<Iterator, Policy>;
    using positive_wrapper_t = positive_parse_result_t<Iterator, Policy>;

    static auto extract_string(size_t consumed, Iterator from, Iterator to)
        -> optional_wrapper_t {
        return optional_wrapper_t{positive_wrapper_t{consumed}};
    }

    static auto extract_error(parse_result_mapper_t<Iterator, Policy> *string)
        -> optional_wrapper_t {
        return optional_wrapper_t{positive_wrapper_t{string->consumed}};
    }

    static auto extract_int(parse_result_mapper_t<Iterator, Policy> *string)
        -> optional_wrapper_t {
        return optional_wrapper_t{positive_wrapper_t{string->consumed}};
    }

    static auto extract_nil(size_t consumed,
                            markers::string_t<Iterator> &string)
        -> optional_wrapper_t {
        return optional_wrapper_t{positive_wrapper_t{consumed}};
    }

    static auto extract_bulk_string(size_t already_consumed, size_t shift,
                                    int count, Iterator from, Iterator to)
        -> optional_wrapper_t {
        auto head = from + shift;
        size_t left = std::distance(head, to);
        size_t ucount = static_cast<size_t>(count);
        const auto &terminator = get_terminator();
        auto terminator_size = terminator.size();
        if (left < ucount + terminator_size) {
            return not_enough_data_t{};
        }
        auto tail = head + ucount;
        auto tail_end = tail + terminator_size;

        auto from_t(get_terminator().cbegin()), to_t(get_terminator().cend());
        bool found_terminator = std::equal(tail, tail_end, from_t, to_t);
        if (!found_terminator) {
            throw std::runtime_error("Terminator not found for bulk string");
        }
        size_t consumed = shift + count + terminator_size + already_consumed;

        return optional_wrapper_t{positive_wrapper_t{consumed}};
    }

    static auto extract_array(size_t already_consumed, size_t shift, int count,
                              Iterator from, Iterator to)
        -> optional_wrapper_t {

        using positive_wrapper_t = positive_parse_result_t<Iterator, Policy>;

        auto result = markers::array_holder_t<Iterator>{};
        result.elements.reserve(count);
        for (auto i = 0; i < count; ++i) {
            Iterator left = from + shift;
            auto optional_parse_result = raw_parse<Iterator, Policy>(left, to);
            auto *no_enogh_data =
                boost::get<not_enough_data_t>(&optional_parse_result);
            if (no_enogh_data) {
                return *no_enogh_data;
            }
            auto &parsed_data =
                boost::get<positive_wrapper_t>(optional_parse_result);
            shift += parsed_data.consumed;
        }
        return optional_wrapper_t{positive_wrapper_t{shift + already_consumed}};
    }
};

template <typename P, typename T> struct Extractor {};

template <typename Policy> struct Extractor<Policy, extractor_tags::e_string> {

    template <typename Iterator>
    optional_parse_result_t<Iterator, Policy>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {
        using helper = ExtractorHelper<Iterator, Policy>;

        auto from_t(get_terminator().cbegin()), to_t(get_terminator().cend());
        auto found_terminator = std::search(from, to, from_t, to_t);

        if (found_terminator == to) {
            return not_enough_data_t{};
        } else {
            size_t consumed = already_consumed +
                              std::distance(from, found_terminator) +
                              get_terminator().size();
            return helper::extract_string(consumed, from, found_terminator);
        }
    }
};

template <typename Policy> struct Extractor<Policy, extractor_tags::e_error> {
    template <typename Iterator>
    optional_parse_result_t<Iterator, Policy>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {
        using helper = ExtractorHelper<Iterator, Policy>;

        Extractor<Policy, extractor_tags::e_string> e;
        auto parse_result = e(from, to, already_consumed);

        auto *positive_result =
            boost::get<parse_result_mapper_t<Iterator, Policy>>(&parse_result);
        if (!positive_result) {
            return not_enough_data_t{};
        } else {
            return helper::extract_error(positive_result);
        }
    }
};

template <typename Policy> struct Extractor<Policy, extractor_tags::e_int> {
    template <typename Iterator>
    optional_parse_result_t<Iterator, Policy>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {
        using helper = ExtractorHelper<Iterator, Policy>;

        Extractor<Policy, extractor_tags::e_string> e;
        auto parse_result = e(from, to, already_consumed);

        auto *positive_result =
            boost::get<parse_result_mapper_t<Iterator, Policy>>(&parse_result);
        if (!positive_result) {
            return not_enough_data_t{};
        } else {
            return helper::extract_int(positive_result);
        }
    }
};

template <typename Policy>
struct Extractor<Policy, extractor_tags::e_bulk_string> {
    template <typename Iterator>
    optional_parse_result_t<Iterator, Policy>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {
        using helper = ExtractorHelper<Iterator, Policy>;
        using KeepPolicy = parsing_policy::keep_result;

        Extractor<KeepPolicy, extractor_tags::e_string> e;

        auto parse_result = e(from, to, 0);
        auto *positive_result =
            boost::get<parse_result_mapper_t<Iterator, KeepPolicy>>(
                &parse_result);
        if (!positive_result) {
            return not_enough_data_t{};
        }

        auto &count_string =
            boost::get<markers::string_t<Iterator>>(positive_result->result);
        std::string s{count_string.from, count_string.to};
        int count = boost::lexical_cast<int>(s);
        if (count == -1) {
            return helper::extract_nil(
                positive_result->consumed + already_consumed, count_string);
        } else if (count < -1) {
            throw std::runtime_error(std::string("Value ") + s +
                                     " in unacceptable for bulk strings");
        } else {
            return helper::extract_bulk_string(
                already_consumed, positive_result->consumed, count, from, to);
        }
    }
};

template <typename Policy> struct Extractor<Policy, extractor_tags::e_array> {
    template <typename Iterator>
    optional_parse_result_t<Iterator, Policy>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {
        using helper = ExtractorHelper<Iterator, Policy>;
        using KeepPolicy = parsing_policy::keep_result;

        Extractor<KeepPolicy, extractor_tags::e_string> e;
        auto parse_result = e(from, to, 0);
        auto *positive_result =
            boost::get<parse_result_mapper_t<Iterator, KeepPolicy>>(
                &parse_result);
        if (!positive_result) {
            return not_enough_data_t{};
        }

        auto &count_string =
            boost::get<markers::string_t<Iterator>>(positive_result->result);
        std::string s{count_string.from, count_string.to};
        int count = boost::lexical_cast<int>(s);
        if (count == -1) {
            return helper::extract_nil(
                positive_result->consumed + already_consumed, count_string);
        } else if (count < -1) {
            throw std::runtime_error(std::string("Value ") + s +
                                     " in unacceptable for arrays");
        } else {
            return helper::extract_array(
                already_consumed, positive_result->consumed, count, from, to);
        }
    }
};

template <typename Iterator, typename Policy>
static optional_parse_result_t<Iterator, Policy> raw_parse(Iterator &from,
                                                           Iterator &to) {
    if (from == to) {
        return not_enough_data_t{};
    }

    auto marker = *from++;

    switch (marker) {
    case '+': {
        Extractor<Policy, extractor_tags::e_string> e;
        return e(from, to, 1);
    }
    case '-': {
        Extractor<Policy, extractor_tags::e_error> e;
        return e(from, to, 1);
    }
    case ':': {
        Extractor<Policy, extractor_tags::e_int> e;
        return e(from, to, 1);
    }
    case '$': {
        Extractor<Policy, extractor_tags::e_bulk_string> e;
        return e(from, to, 1);
    }
    case '*': {
        Extractor<Policy, extractor_tags::e_array> e;
        return e(from, to, 1);
    }
    default: { throw std::runtime_error("wrong introduction"); }
    }
    throw std::runtime_error("wrong introduction");
}

template <typename Iterator, typename Policy>
parse_result_t<Iterator, Policy> Protocol::parse(Iterator &from,
                                                 Iterator &to) noexcept {
    try {
        auto result = raw_parse<Iterator, Policy>(from, to);
        return parse_result_t<Iterator, Policy>{result};
    } catch (std::exception &e) {
        return protocol_error_t{e.what()};
    }
}

std::ostream &Protocol::serialize(std::ostream &buff,
                                  const single_command_t &cmd) {
    buff << '*' << (cmd.arguments.size()) << get_terminator();

    for (const auto &arg : cmd.arguments) {
        buff << '$' << arg.size() << get_terminator() << arg
             << get_terminator();
    }
    return buff;
}
};
