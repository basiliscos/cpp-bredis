//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <boost/core/enable_if.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/variant.hpp>
#include <vector>

#include "Result.hpp"

namespace bredis {

namespace detail {

template <bool...> struct bool_pack;
template <bool... bs>
using all_true = std::is_same<bool_pack<bs..., true>, bool_pack<true, bs...>>;

template <class R, class... Ts>
using are_all_constructible = all_true<std::is_constructible<R, Ts>::value...>;

} // namespace detail

using args_container_t = std::vector<boost::string_ref>;
struct single_command_t {
    args_container_t arguments;

    template <typename... Args,
              typename = boost::enable_if_t<detail::are_all_constructible<
                  boost::string_ref, Args...>::value>>
    single_command_t(Args &&... args) : arguments{std::forward<Args>(args)...} {
        static_assert(sizeof...(Args) >= 1, "Empty command is not allowed");
    }

    template <typename InputIterator,
              typename = boost::enable_if_t<std::is_constructible<
                  boost::string_ref, typename std::iterator_traits<
                                         InputIterator>::value_type>::value>>
    single_command_t(InputIterator first, InputIterator last)
        : arguments(first, last) {}
};

using command_container_t = std::vector<single_command_t>;

using command_wrapper_t = boost::variant<single_command_t, command_container_t>;

} // namespace bredis
