//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <algorithm>
#include <string>

#include "Markers.hpp"

namespace bredis {

namespace marker_helpers {

template <typename Iterator>

class equality : public boost::static_visitor<bool> {
  private:
    const char *begin_;
    const char *end_;

  public:
    equality(std::string str)
        : begin_(str.c_str()), end_(str.c_str() + str.size()) {}

    template <typename T> bool operator()(const T &value) const {
        return false;
    }

    bool operator()(const markers::string_t<Iterator> &value) const {
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
