//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <boost/variant.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <vector>

namespace bredis {

namespace markers {

template <typename Iterator> struct string_t {
    using iterator_t = Iterator;
    Iterator from;
    Iterator to;
};

template <typename Iterator> struct error_t {
    using iterator_t = Iterator;
    string_t<Iterator> string;
};

template <typename Iterator> struct int_t {
    using iterator_t = Iterator;
    string_t<Iterator> string;
};

template <typename Iterator> struct nil_t {
    using iterator_t = Iterator;
    string_t<Iterator> string;
};

template <typename Iterator> struct array_holder_t;

template <typename Iterator>
using array_wrapper_t = boost::recursive_wrapper<array_holder_t<Iterator>>;

template <typename Iterator>
using redis_result_t =
    boost::variant<int_t<Iterator>, string_t<Iterator>, error_t<Iterator>,
                   nil_t<Iterator>, array_wrapper_t<Iterator>>;

template <typename Iterator> struct array_holder_t {
    using iterator_t = Iterator;
    using recursive_array_t = std::vector<redis_result_t<Iterator>>;
    recursive_array_t elements;
};

} // namespace markers

} // namespace bredis
