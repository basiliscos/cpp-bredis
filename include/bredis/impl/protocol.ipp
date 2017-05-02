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

const std::string Protocol::terminator = "\r\n";

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
    operator()(Iterator &from, Iterator &to, int32_t already_consumed) const {

        auto from_t(Protocol::terminator.cbegin()),
            to_t(Protocol::terminator.cend());
        ;
        auto found_terminator = std::search(from, to, from_t, to_t);

        if (found_terminator == to) {
            return no_enogh_data_t{};
        } else {
            int32_t consumed = already_consumed +
                               std::distance(from, found_terminator) +
                               Protocol::terminator.size();
            return positive_parse_result_t<Iterator>{
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
    operator()(Iterator &from, Iterator &to, int32_t already_consumed) const {

        Extractor<extractor_tags::e_string> e;
        auto parse_result = e(from, to, already_consumed);
        auto *positive_result =
            boost::get<positive_parse_result_t<Iterator>>(&parse_result);
        if (!positive_result) {
            return no_enogh_data_t{};
        } else {
            auto &string_result = boost::get<markers::string_t<Iterator>>(
                positive_result->result);
            return positive_parse_result_t<Iterator>{
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
    operator()(Iterator &from, Iterator &to, int32_t already_consumed) const {

        Extractor<extractor_tags::e_string> e;
        auto parse_result = e(from, to, already_consumed);
        auto *positive_result =
            boost::get<positive_parse_result_t<Iterator>>(&parse_result);
        if (!positive_result) {
            return no_enogh_data_t{};
        } else {
            auto &string_result = boost::get<markers::string_t<Iterator>>(
                positive_result->result);
            return positive_parse_result_t<Iterator>{
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
    operator()(Iterator &from, Iterator &to, int32_t already_consumed) const {
        Extractor<extractor_tags::e_string> e;
        auto parse_result = e(from, to, 0);
        auto *positive_result =
            boost::get<positive_parse_result_t<Iterator>>(&parse_result);
        if (!positive_result) {
            return no_enogh_data_t{};
        }

        auto &count_string =
            boost::get<markers::string_t<Iterator>>(positive_result->result);
        std::string s;
        s.reserve(positive_result->consumed);
        s.append(count_string.from, count_string.to);
        int count = boost::lexical_cast<std::size_t>(s);
        if (count == -1) {
            return positive_parse_result_t<Iterator>{
                positive_parse_result_t<Iterator>{
                    markers::redis_result_t<Iterator>{
                        markers::nil_t<Iterator>{count_string}},
                    positive_result->consumed + already_consumed}};
        } else if (count < -1) {
            throw std::runtime_error(std::string("Value ") + s +
                                     " in unacceptable for bulk strings");
        } else {
            auto head = from + positive_result->consumed;
            auto left = std::distance(head, to);
            if (left < count + Protocol::terminator.size()) {
                return no_enogh_data_t{};
            }
            auto tail = head + count;
            std::string debug_str;
            debug_str.append(head, tail);
            const char *ds = debug_str.c_str();

            auto from_t(Protocol::terminator.cbegin()),
                to_t(Protocol::terminator.cend());
            auto found_terminator = std::search(tail, to, from_t, to_t);
            if (found_terminator != tail) {
                throw std::runtime_error(
                    "Terminator not found for bulk string");
            }
            int32_t consumed = positive_result->consumed + count +
                               Protocol::terminator.size() + already_consumed;
            return positive_parse_result_t<Iterator>{
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
    operator()(Iterator &from, Iterator &to, int32_t already_consumed) const {
        Extractor<extractor_tags::e_string> e;
        auto parse_result = e(from, to, 0);
        auto *positive_result =
            boost::get<positive_parse_result_t<Iterator>>(&parse_result);
        if (!positive_result) {
            return no_enogh_data_t{};
        }

        auto &count_string =
            boost::get<markers::string_t<Iterator>>(positive_result->result);
        std::string s;
        s.reserve(positive_result->consumed);
        s.append(count_string.from, count_string.to);
        int count = boost::lexical_cast<std::size_t>(s);
        if (count == -1) {
            return positive_parse_result_t<Iterator>{
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
            int32_t consumed = positive_result->consumed;
            for (auto i = 0; i < count; ++i) {
                Iterator left = from + consumed;
                auto optional_parse_result = raw_parse(left, to);
                auto *no_enogh_data =
                    boost::get<no_enogh_data_t>(&optional_parse_result);
                if (no_enogh_data) {
                    return *no_enogh_data;
                }
                auto &parsed_data =
                    boost::get<positive_parse_result_t<Iterator>>(
                        optional_parse_result);
                result.elements.emplace_back(parsed_data.result);
                consumed += parsed_data.consumed;
            }
            return positive_parse_result_t<Iterator>{
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
        return no_enogh_data_t{};
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

template <typename ConstBufferSequence>
parse_result_t<boost::asio::buffers_iterator<ConstBufferSequence, char>>
Protocol::parse(const ConstBufferSequence &buff) noexcept {

    using Iterator = boost::asio::buffers_iterator<ConstBufferSequence, char>;

    try {
        auto begin = Iterator::begin(buff);
        auto end = Iterator::end(buff);
        return raw_parse(begin, end);
    } catch (std::exception &e) {
        return protocol_error_t{e.what()};
    }
}

std::ostream &Protocol::serialize(std::ostream &buff,
                                  const single_command_t &cmd) {
    buff << '*' << (cmd.arguments.size()) << Protocol::terminator;

    for (const auto &arg : cmd.arguments) {
        buff << '$' << arg.size() << Protocol::terminator << arg
             << Protocol::terminator;
    }
    return buff;
}
};
