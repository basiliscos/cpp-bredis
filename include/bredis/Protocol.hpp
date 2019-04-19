//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <boost/asio/buffers_iterator.hpp>
#include <ostream>
#include <string>

#include "Command.hpp"
#include "Result.hpp"

namespace bredis {

class Protocol {
  public:
    template <typename Iterator, typename Policy = parsing_policy::keep_result>
    static inline parse_result_t<Iterator, Policy> parse(const Iterator &from,
                                                         const Iterator &to);

    template <typename DynamicBuffer>
    static inline void serialize(DynamicBuffer &buff,
                                 const single_command_t &cmd);
};

} // namespace bredis

#include "impl/protocol.ipp"
