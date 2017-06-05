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
    static const bredis_category category;

  public:
    static inline boost::system::error_code make_error_code(bredis_errors e);
};

const bredis_category Error::category;

boost::system::error_code Error::make_error_code(bredis_errors e) {
    return boost::system::error_code(static_cast<int>(e), category);
}

} // namespace bredis
