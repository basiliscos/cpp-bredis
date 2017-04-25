//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <boost/lexical_cast.hpp>

namespace bredis {

using optional_string_t = boost::variant<boost::string_ref, nil_t>;
using optional_array_t = boost::variant<array_holder_t, nil_t>;

const std::string Protocol::terminator = "\r\n";

template <typename T> struct extraction_result_t {
    T value;
    size_t consumed;
};

using some_parse_result_t =
    boost::variant<no_enoght_data_t, positive_parse_result_t>;

static some_parse_result_t raw_parse(const boost::string_ref &outer_range);

template <typename T> struct extractor {};

template <> struct extractor<string_result_t> {
    extraction_result_t<string_result_t>
    operator()(const boost::string_ref &range) const {
        auto location = range.find(Protocol::terminator);
        extraction_result_t<string_result_t> r{boost::string_ref(), 0};
        if (location != std::string::npos) {
            r.consumed = location + Protocol::terminator.size();
            r.value = boost::string_ref(range.data(), location);
        }
        return r;
    }
};

template <> struct extractor<int_result_t> {
    extraction_result_t<int_result_t>
    operator()(const boost::string_ref &range) const {
        extraction_result_t<int_result_t> r{0, 0};
        auto string_e = extractor<string_result_t>()(range);
        if (string_e.consumed) {
            r.consumed = string_e.consumed;
            r.value = boost::lexical_cast<int_result_t>(string_e.value);
        }
        return r;
    }
};

template <> struct extractor<optional_string_t> {
    extraction_result_t<optional_string_t>
    operator()(const boost::string_ref &range) const {
        extraction_result_t<optional_string_t> r{optional_string_t(), 0};
        auto int_e = extractor<int_result_t>()(range);
        if (int_e.consumed) {
            auto prefix = int_e.value;
            if (prefix == -1) {
                r.value = optional_string_t(nil_t{});
                r.consumed = int_e.consumed;
            } else if (prefix < -1) {
                throw std::runtime_error(
                    std::string("Value ") +
                    boost::lexical_cast<std::string>(prefix) +
                    " in unacceptable for bulk strings");
            } else if (int_e.consumed + prefix + Protocol::terminator.size() <=
                       range.size()) {
                boost::string_ref str_range(range.data() + int_e.consumed,
                                            prefix);
                r.value = optional_string_t{str_range};
                r.consumed =
                    int_e.consumed + prefix + Protocol::terminator.size();
            }
        }
        return r;
    }
};

template <> struct extractor<optional_array_t> {
    extraction_result_t<optional_array_t>
    operator()(const boost::string_ref &range) const {
        extraction_result_t<optional_array_t> r{optional_array_t(), 0};
        auto int_e = extractor<int_result_t>()(range);
        if (int_e.consumed) {
            auto prefix = int_e.value;
            if (prefix == -1) {
                r.value = optional_array_t(nil_t{});
                r.consumed = int_e.consumed;
            } else if (prefix < -1) {
                throw std::runtime_error(
                    std::string("Value ") +
                    boost::lexical_cast<std::string>(prefix) +
                    " in unacceptable for arrays");
            } else {
                auto value = array_holder_t{};
                auto consumed = int_e.consumed;
                ;
                for (auto i = 0; i < prefix; ++i) {
                    boost::string_ref left_range(range.data() + consumed,
                                                 range.size() - consumed);
                    auto raw_result = raw_parse(left_range);
                    if (boost::get<no_enoght_data_t>(&raw_result)) {
                        return r;
                    }
                    positive_parse_result_t something(
                        boost::get<positive_parse_result_t>(raw_result));
                    consumed += something.consumed;
                    value.elements.emplace_back(std::move(something.result));
                }
                r.value = std::move(value);
                r.consumed = consumed;
            }
        }
        return r;
    }
};

template <typename T, typename P, bool = false> struct transform_t {
    some_parse_result_t operator()(extraction_result_t<T> &e) const {
        if (!e.consumed) {
            return no_enoght_data_t{};
        } else {
            return positive_parse_result_t{P::value(e.value), e.consumed + 1};
        }
    }
};

template <typename T, typename W> struct WrapPolicy {
    using type_t = T;
    static inline redis_result_t value(T &v) {
        return redis_result_t(W{std::move(v)});
    }
};

template <typename T> struct MovePolicy {
    using type_t = T;
    static inline redis_result_t value(T &v) { return redis_result_t(T{v}); }
};

struct ArrayUnwrapPolicy {
    using type_t = array_holder_t;
    static inline redis_result_t value(type_t &v) { return redis_result_t(v); }
};

template <typename T, typename P>
class unpack_visitor : public boost::static_visitor<redis_result_t> {
  public:
    redis_result_t operator()(nil_t &nil) const { return redis_result_t(nil); }

    redis_result_t operator()(T &v) const { return P::value(v); }
};

template <typename T, typename P> struct transform_t<T, P, true> {
    using underlying_t = typename P::type_t;
    some_parse_result_t operator()(extraction_result_t<T> &e) const {
        if (!e.consumed) {
            return no_enoght_data_t{};
        } else {
            auto value = boost::apply_visitor(unpack_visitor<underlying_t, P>(),
                                              e.value);
            return positive_parse_result_t{std::move(value), e.consumed + 1};
        }
    }
};

static some_parse_result_t raw_parse(const boost::string_ref &outer_range) {
    if (outer_range.size() < 1) {
        return no_enoght_data_t{};
    }

    auto marker = outer_range[0];
    boost::string_ref range(outer_range.data() + 1, outer_range.size() - 1);

    switch (marker) {
    case '+': {
        using policy_t = WrapPolicy<string_result_t, string_holder_t>;
        extractor<string_result_t> e;
        transform_t<string_result_t, policy_t> t;
        auto v = e(range);
        return std::move(t(v));
    }
    case '-': {
        using policy_t = WrapPolicy<string_result_t, error_holder_t>;
        extractor<string_result_t> e;
        transform_t<string_result_t, policy_t> t;
        auto v = e(range);
        return std::move(t(v));
    }
    case ':': {
        using policy_t = MovePolicy<int_result_t>;
        extractor<int_result_t> e;
        transform_t<int_result_t, policy_t> t;
        auto v = e(range);
        return std::move(t(v));
    }
    case '$': {
        using policy_t = WrapPolicy<string_result_t, string_holder_t>;
        extractor<optional_string_t> e;
        transform_t<optional_string_t, policy_t, true> t;
        auto v = e(range);
        return std::move(t(v));
    }
    case '*': {
        using policy_t = ArrayUnwrapPolicy;
        extractor<optional_array_t> e;
        transform_t<optional_array_t, policy_t, true> t;
        auto v = e(range);
        return std::move(t(v));
    }
    default: { throw std::runtime_error("wrong introduction"); }
    }
}

parse_result_t Protocol::parse(const std::string &buff) noexcept {
    boost::string_ref range(buff.c_str(), buff.size());
    return parse(range);
}

parse_result_t Protocol::parse(const boost::string_ref &buff) noexcept {
    try {
        return raw_parse(buff);
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
