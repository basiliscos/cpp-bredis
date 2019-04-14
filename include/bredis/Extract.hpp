//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <iterator>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/lexical_cast.hpp>
#include <boost/variant.hpp>
#include <boost/variant/recursive_variant.hpp>

#include "bredis/Result.hpp"

namespace bredis {

namespace extracts {

struct string_t {
    std::string str;
};

struct error_t {
    std::string str;
};

using int_t = int64_t;

struct nil_t {};

// forward declaration
struct array_holder_t;
using array_wrapper_t = boost::recursive_wrapper<array_holder_t>;

using extraction_result_t =
    boost::variant<int_t, string_t, error_t, nil_t, array_wrapper_t>;

struct array_holder_t {
    using recursive_array_t = std::vector<extraction_result_t>;
    recursive_array_t elements;
};

} // namespace extracts

template <typename Iterator>
struct extractor : public boost::static_visitor<extracts::extraction_result_t> {

    extracts::extraction_result_t
    operator()(const markers::string_t<Iterator> &value) const {
        extracts::string_t r;
        auto size = std::distance(value.from, value.to);
        r.str.reserve(size);
        r.str.append(value.from, value.to);
        return r;
    }

    extracts::extraction_result_t
    operator()(const markers::error_t<Iterator> &value) const {
        extracts::error_t r;
        auto size = std::distance(value.string.from, value.string.to);
        r.str.reserve(size);
        r.str.append(value.string.from, value.string.to);
        return r;
    }

    extracts::extraction_result_t
    operator()(const markers::int_t<Iterator> &value) const {
        std::string str;
        auto size = std::distance(value.string.from, value.string.to);
        str.reserve(size);
        str.append(value.string.from, value.string.to);
        return extracts::int_t{boost::lexical_cast<extracts::int_t>(str)};
    }

    extracts::extraction_result_t
    operator()(const markers::nil_t<Iterator> & /*value*/) const {
        return extracts::nil_t{};
    }

    extracts::extraction_result_t
    operator()(const markers::array_holder_t<Iterator> &value) const {
        extracts::array_holder_t r;
        r.elements.reserve(value.elements.size());
        for (const auto &v : value.elements) {
            r.elements.emplace_back(boost::apply_visitor(*this, v));
        }
        return r;
    }
};

} // namespace bredis
