//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>

#include <boost/variant.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/system/error_code.hpp>

namespace bredis {

using int_result_t = int64_t;
using string_result_t = boost::string_ref;
using args_container_t = std::vector<string_result_t>;

struct array_holder_t;
using array_result_t = array_holder_t;
using array_wrapper_t = boost::recursive_wrapper<array_result_t>;

struct protocol_error_t {
    std::string what;
    bool operator==(const char *rhs) { return what == rhs; }
};

struct string_holder_t {
    string_result_t str;
    bool operator==(const char *rhs) { return str == rhs; }
};

struct error_holder_t {
    string_result_t str;
    bool operator==(const char *rhs) { return str == rhs; }
};

struct nil_t {
    bool operator==(const nil_t &rhs) { return true; }
};

using some_result_t =
    boost::variant<int_result_t, string_holder_t, error_holder_t, nil_t,
                   array_wrapper_t, protocol_error_t>;

struct array_holder_t {
    using recursive_array_t = std::vector<some_result_t>;
    recursive_array_t elements;
};

struct parse_result_t {
    some_result_t result;
    size_t consumed;
};

using redis_result_t = boost::variant<int_result_t, string_holder_t,
                                      error_holder_t, nil_t, array_wrapper_t>;

using command_callback_t = std::function<void(
    const boost::system::error_code &error_code, redis_result_t &&result)>;
};
