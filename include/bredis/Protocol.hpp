//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <ostream>
#include <string>

#include "Command.hpp"
#include "Result.hpp"

namespace bredis {

class Protocol {
  public:
    static const std::string terminator;
    static inline parse_result_t parse(const boost::string_ref &buff) noexcept;
    static std::ostream &serialize(std::ostream &buff,
                                   const single_command_t &cmd);
};
};

#include "impl/protocol.ipp"
