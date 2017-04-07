//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <string>
#include <ostream>

#include "Result.hpp"

namespace bredis {

class Protocol {
  public:
    static const std::string terminator;
    static inline parse_result_t parse(const std::string &buff) noexcept;
    static inline parse_result_t parse(const boost::string_ref &buff) noexcept;
    static std::ostream &serialize(std::ostream &buff, const std::string &cmd,
                                   const args_container_t &args);
};
};

#include "impl/protocol.hpp"
