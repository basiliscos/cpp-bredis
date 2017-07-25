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

enum class bredis_errors {
    wrong_intoduction = 1,
    parser_error,
    count_conversion,
    count_range,
    bulk_terminator
};

class bredis_category : public boost::system::error_category {
  public:
    const char *name() const noexcept { return "bredis"; }
    std::string message(int ev) const {
        auto ec = static_cast<bredis_errors>(ev);
        switch (ec) {
        case bredis_errors::wrong_intoduction:
            return "Wrong introduction";
        case bredis_errors::parser_error:
            return "Parser error";
        case bredis_errors::count_conversion:
            return "Cannot convert count to number";
        case bredis_errors::count_range:
            return "Unacceptable count value";
        case bredis_errors::bulk_terminator:
            return "Terminator for bulk string not found";
        }
        return "Unknown protocol error";
    }
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
