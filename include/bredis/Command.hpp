//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <memory>
#include <boost/utility/string_ref.hpp>
#include <boost/variant.hpp>
#include <boost/asio/buffer.hpp>
#include <type_traits>
#include <vector>
#include <cstdio>
#include <cassert>

#include "Result.hpp"

namespace bredis {

namespace detail {

template <bool...> struct bool_pack;
template <bool... bs>
using all_true = std::is_same<bool_pack<bs..., true>, bool_pack<true, bs...>>;

template <class R, class... Ts>
using are_all_constructible = all_true<std::is_constructible<R, Ts>::value...>;

} // namespace detail

using output_buff_t = std::vector<boost::asio::const_buffer>;
using args_container_t = std::vector<boost::string_ref>;
// according to https://redis.io/topics/protocol max string bulk size is 512mb
// So, 512*1024^2 = "536870912" = 9 chars
// prefix char is 1 byte, and postfix (terminator size) is 2 bytes
// + 1 zero byte
// 9 + 1 + 2 + 1 = 14
const constexpr std::uint32_t per_arg = 14;

struct single_command_t {
    using buff_t = std::unique_ptr<char[]>;

    //explicit single_command_t() {}

    template <typename... Args,
              typename = std::enable_if_t<detail::are_all_constructible<
                  boost::string_ref, Args...>::value>>
    single_command_t(Args &&... args) : arguments{std::forward<Args>(args)...} {
        /*
        constexpr std::uint32_t args_count = sizeof...(Args);
        static_assert(args_count >= 1, "Empty command is not allowed");
        */
    }

    template <typename InputIterator,
              typename = std::enable_if_t<std::is_constructible<
                  boost::string_ref, typename std::iterator_traits<
                                         InputIterator>::value_type>::value>>
    single_command_t(InputIterator first, InputIterator last)
        : arguments(first, last) {}


    inline std::uint32_t buffers_count() const {
        return static_cast<std::uint32_t>(arguments.size()) + 1;
    }

    inline void prepare(output_buff_t& accumulator) {
        std::uint32_t count = buffers_count();
        assert((count >= 2) && "Empty command is not allowed");
        buff = buff_t(new char[count  * per_arg]{0});
        char* ptr = buff.get();
        int arg_sz = std::sprintf(ptr, "*%u", count - 1);
        assert(arg_sz > 0);
        accumulator.emplace_back(ptr, arg_sz);
        ptr += arg_sz;
        for(std::size_t idx = 0; idx < arguments.size(); ++idx) {
            const auto& arg = arguments[idx];
            arg_sz  = sprintf(ptr, "\r\n$%u\r\n", static_cast<std::uint32_t>(arg.size()));
            assert(arg_sz > 0);
            accumulator.emplace_back(ptr, arg_sz);
            ptr += arg_sz;
            accumulator.emplace_back(arg.data(), arg.size());
        }
        accumulator.emplace_back("\r\n", 2);
    }

    const args_container_t arguments;
private:
    buff_t buff;
};

using command_container_t = std::vector<single_command_t>;

using command_wrapper_t = boost::variant<single_command_t, command_container_t>;

} // namespace bredis
