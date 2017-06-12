//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <boost/system/error_code.hpp>
#include <string>

namespace bredis {

enum bredis_errors { protocol_error = 1 };

class bredis_category : public boost::system::error_category {
  public:
    const char *name() const noexcept { return "bredis"; }
    std::string message(int ev) const { return "protocol error"; }
};

class Error {

  public:
    Error(){};
    static inline boost::system::error_code make_error_code(bredis_errors e);
    static inline bredis_category const &get_error_category() {
        static bredis_category const cat{};
        return cat;
    }
};

boost::system::error_code Error::make_error_code(bredis_errors e) {
    return boost::system::error_code(static_cast<int>(e),
                                     Error::get_error_category());
}

} // namespace bredis
