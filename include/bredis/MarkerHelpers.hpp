//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <algorithm>
#include <iterator>
#include <string>

#include "Markers.hpp"

namespace bredis {

namespace marker_helpers {

template <typename Iterator>
struct stringizer : public boost::static_visitor<std::string> {

    std::string operator()(const markers::string_t<Iterator> &value) const {
        auto size = std::distance(value.from, value.to);
        std::string r;
        r.reserve(size);
        r.append(value.from, value.to);
        return "[str] " + r;
    }

    std::string operator()(const markers::error_t<Iterator> &value) const {
        auto size = std::distance(value.string.from, value.string.to);
        std::string r;
        r.reserve(size);
        r.append(value.string.from, value.string.to);
        return "[err] " + r;
    }

    std::string operator()(const markers::int_t<Iterator> &value) const {
        auto size = std::distance(value.string.from, value.string.to);
        std::string r;
        r.reserve(size);
        r.append(value.string.from, value.string.to);
        return "[int] " + r;
    }

    std::string operator()(const markers::nil_t<Iterator> &value) const {
        return "[nil] ";
    }

    std::string
    operator()(const markers::array_holder_t<Iterator> &value) const {
        std::string r = "[array] {";
        for (const auto &v : value.elements) {
            r += boost::apply_visitor(*this, v) + ", ";
        }
        r += "}";
        return r;
    }
};

template <typename Iterator>
class equality : public boost::static_visitor<bool> {
    using StringIterator = std::string::const_iterator;

  private:
    std::string copy_;
    StringIterator begin_;
    StringIterator end_;

  public:
    equality(std::string str)
        : copy_(str), begin_(std::begin(copy_)), end_(std::end(copy_)) {}

    template <typename T> bool operator()(const T &value) const {
        return false;
    }

    bool operator()(const markers::string_t<Iterator> &value) const {
        auto helper = stringizer<Iterator>();
        auto str = helper(value);
        return std::equal(begin_, end_, value.from, value.to);
    }

    bool operator()(const markers::int_t<Iterator> &value) const {
        return std::equal(begin_, end_, value.string.from, value.string.to);
    }

    bool operator()(const markers::error_t<Iterator> &value) const {
        return std::equal(begin_, end_, value.string.from, value.string.to);
    }

    bool operator()(const markers::nil_t<Iterator> &value) const {
        return std::equal(begin_, end_, value.string.from, value.string.to);
    }
};

} // marker_helpers

} // bredis
