//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <boost/utility/string_ref.hpp>
#include <boost/variant.hpp>
#include <vector>

#include "Result.hpp"

namespace bredis {

using args_container_t = std::vector<boost::string_ref>;

struct single_command_t {
    args_container_t arguments;

    template <typename... Args>
    single_command_t(Args &&... args) : arguments{std::forward<Args>(args)...} {
        static_assert(sizeof...(Args) >= 1, "Empty command is not allowed");
    }

    single_command_t(const single_command_t&) = default;
    single_command_t(single_command_t&&) = default;
};

using command_container_t = std::vector<single_command_t>;

using command_wrapper_t = boost::variant<single_command_t, command_container_t>;

} // namespace bredis
