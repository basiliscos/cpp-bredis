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

namespace bredis {

static inline const std::string &get_terminator() {
    static std::string terminator("\r\n");
    return terminator;
}

template <typename Iterator>
static optional_parse_result_t<Iterator> raw_parse(Iterator &from,
                                                   Iterator &to);

namespace extractor_tags {
struct e_string;
struct e_error;
struct e_int;
struct e_bulk_string;
struct e_array;
}; // extractors

template <typename T> struct Extractor {};

template <> struct Extractor<extractor_tags::e_string> {

    template <typename Iterator>
    optional_parse_result_t<Iterator>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {

        auto from_t(get_terminator().cbegin()), to_t(get_terminator().cend());
        auto found_terminator = std::search(from, to, from_t, to_t);

        if (found_terminator == to) {
            return not_enough_data_t{};
        } else {
            size_t consumed = already_consumed +
                              std::distance(from, found_terminator) +
                              get_terminator().size();
            return optional_parse_result_t<Iterator>{
                positive_parse_result_t<Iterator>{
                    markers::redis_result_t<Iterator>{
                        markers::string_t<Iterator>{from, found_terminator}},
                    consumed}};
        }
    }
};

template <> struct Extractor<extractor_tags::e_error> {
    template <typename Iterator>
    optional_parse_result_t<Iterator>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {

        Extractor<extractor_tags::e_string> e;
        auto parse_result = e(from, to, already_consumed);
        auto *positive_result =
            boost::get<positive_parse_result_t<Iterator>>(&parse_result);
        if (!positive_result) {
            return not_enough_data_t{};
        } else {
            auto &string_result = boost::get<markers::string_t<Iterator>>(
                positive_result->result);
            return optional_parse_result_t<Iterator>{
                positive_parse_result_t<Iterator>{
                    markers::redis_result_t<Iterator>{
                        markers::error_t<Iterator>{string_result}},
                    positive_result->consumed}};
        }
    }
};

template <> struct Extractor<extractor_tags::e_int> {
    template <typename Iterator>
    optional_parse_result_t<Iterator>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {

        Extractor<extractor_tags::e_string> e;
        auto parse_result = e(from, to, already_consumed);
        auto *positive_result =
            boost::get<positive_parse_result_t<Iterator>>(&parse_result);
        if (!positive_result) {
            return not_enough_data_t{};
        } else {
            auto &string_result = boost::get<markers::string_t<Iterator>>(
                positive_result->result);
            return optional_parse_result_t<Iterator>{
                positive_parse_result_t<Iterator>{
                    markers::redis_result_t<Iterator>{
                        markers::int_t<Iterator>{string_result}},
                    positive_result->consumed}};
        }
    }
};

template <> struct Extractor<extractor_tags::e_bulk_string> {
    template <typename Iterator>
    optional_parse_result_t<Iterator>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {
        Extractor<extractor_tags::e_string> e;
        auto parse_result = e(from, to, 0);
        auto *positive_result =
            boost::get<positive_parse_result_t<Iterator>>(&parse_result);
        if (!positive_result) {
            return not_enough_data_t{};
        }

        auto &count_string =
            boost::get<markers::string_t<Iterator>>(positive_result->result);
        std::string s{count_string.from, count_string.to};
        int count = boost::lexical_cast<int>(s);
        if (count == -1) {
            return optional_parse_result_t<Iterator>{
                positive_parse_result_t<Iterator>{
                    markers::redis_result_t<Iterator>{
                        markers::nil_t<Iterator>{count_string}},
                    positive_result->consumed + already_consumed}};
        } else if (count < -1) {
            throw std::runtime_error(std::string("Value ") + s +
                                     " in unacceptable for bulk strings");
        } else {
            auto head = from + positive_result->consumed;
            size_t left = std::distance(head, to);
            size_t ucount = static_cast<size_t>(count);
            const auto &terminator = get_terminator();
            auto terminator_size = terminator.size();
            if (left < ucount + terminator_size) {
                return not_enough_data_t{};
            }
            auto tail = head + ucount;
            auto tail_end = tail + terminator_size;

            auto from_t(get_terminator().cbegin()),
                to_t(get_terminator().cend());
            bool found_terminator = std::equal(tail, tail_end, from_t, to_t);
            if (!found_terminator) {
                throw std::runtime_error(
                    "Terminator not found for bulk string");
            }
            size_t consumed = positive_result->consumed + count +
                              terminator_size + already_consumed;

            return optional_parse_result_t<Iterator>{
                positive_parse_result_t<Iterator>{
                    markers::redis_result_t<Iterator>{
                        markers::string_t<Iterator>{head, tail}},
                    consumed}};
        }
    }
};

template <> struct Extractor<extractor_tags::e_array> {
    template <typename Iterator>
    optional_parse_result_t<Iterator>
    operator()(Iterator &from, Iterator &to, size_t already_consumed) const {
        Extractor<extractor_tags::e_string> e;
        auto parse_result = e(from, to, 0);
        auto *positive_result =
            boost::get<positive_parse_result_t<Iterator>>(&parse_result);
        if (!positive_result) {
            return not_enough_data_t{};
        }

        auto &count_string =
            boost::get<markers::string_t<Iterator>>(positive_result->result);
        std::string s{count_string.from, count_string.to};
        int count = boost::lexical_cast<int>(s);
        if (count == -1) {
            return optional_parse_result_t<Iterator>{
                positive_parse_result_t<Iterator>{
                    markers::redis_result_t<Iterator>{
                        markers::nil_t<Iterator>{count_string}},
                    positive_result->consumed + already_consumed}};
        } else if (count < -1) {
            throw std::runtime_error(std::string("Value ") + s +
                                     " in unacceptable for arrays");
        } else {
            auto result = markers::array_holder_t<Iterator>{};
            result.elements.reserve(count);
            size_t consumed = positive_result->consumed;
            for (auto i = 0; i < count; ++i) {
                Iterator left = from + consumed;
                auto optional_parse_result = raw_parse(left, to);
                auto *no_enogh_data =
                    boost::get<not_enough_data_t>(&optional_parse_result);
                if (no_enogh_data) {
                    return *no_enogh_data;
                }
                auto &parsed_data =
                    boost::get<positive_parse_result_t<Iterator>>(
                        optional_parse_result);
                result.elements.emplace_back(parsed_data.result);
                consumed += parsed_data.consumed;
            }
            return optional_parse_result_t<Iterator>{
                positive_parse_result_t<Iterator>{
                    markers::redis_result_t<Iterator>{result},
                    consumed + already_consumed}};
        }
    }
};

template <typename Iterator>
static optional_parse_result_t<Iterator> raw_parse(Iterator &from,
                                                   Iterator &to) {
    if (from == to) {
        return not_enough_data_t{};
    }

    auto marker = *from++;

    switch (marker) {
    case '+': {
        Extractor<extractor_tags::e_string> e;
        return e(from, to, 1);
    }
    case '-': {
        Extractor<extractor_tags::e_error> e;
        return e(from, to, 1);
    }
    case ':': {
        Extractor<extractor_tags::e_int> e;
        return e(from, to, 1);
    }
    case '$': {
        Extractor<extractor_tags::e_bulk_string> e;
        return e(from, to, 1);
    }
    case '*': {
        Extractor<extractor_tags::e_array> e;
        return e(from, to, 1);
    }
    default: { throw std::runtime_error("wrong introduction"); }
    }
}

template <typename Iterator>
parse_result_t<Iterator> Protocol::parse(Iterator &from,
                                         Iterator &to) noexcept {
    try {
        auto result = raw_parse(from, to);
        return parse_result_t<Iterator>{result};
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
