//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include <boost/asio/buffers_iterator.hpp>
#include <boost/system/error_code.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/variant.hpp>

#include "Markers.hpp"

namespace bredis {

template <typename DynamicBuffer> struct to_iterator {
    using iterator_t = boost::asio::buffers_iterator<
        typename DynamicBuffer::const_buffers_type, char>;
};

struct protocol_error_t {
    std::string what;
    bool operator==(const char *rhs) { return what == rhs; }
};

struct not_enough_data_t {};

template <typename Iterator> struct positive_parse_result_t {
    markers::redis_result_t<Iterator> result;
    size_t consumed;
};

template <typename Iterator>
using optional_parse_result_t =
    boost::variant<not_enough_data_t, positive_parse_result_t<Iterator>>;

template <typename Iterator>
using parse_result_t =
    boost::variant<not_enough_data_t, positive_parse_result_t<Iterator>,
                   protocol_error_t>;

} // namespace bredis
