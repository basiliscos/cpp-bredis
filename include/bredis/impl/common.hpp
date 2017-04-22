//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <iostream>

#ifdef BREDIS_DEBUG
#define BREDIS_LOG_DEBUG(msg)                                                  \
    { std::cout << __FILE__ << ":" << __LINE__ << " :: " << msg << std::endl; };
#else
#define BREDIS_LOG_DEBUG(msg) ;
#endif

namespace bredis {

typedef boost::asio::buffers_iterator<
    boost::asio::streambuf::const_buffers_type> asio_iterator;

std::pair<asio_iterator, bool> match_result(asio_iterator begin,
                                            asio_iterator end) {
    const char *char_ptr = &*begin;
    auto size = std::distance(begin, end);
    boost::string_ref data(char_ptr, size);
    auto parse_result = Protocol::parse(data);
    bool has_result =
        (parse_result.consumed > 0) ||
        (boost::get<protocol_error_t>(&parse_result.result) != nullptr);
    BREDIS_LOG_DEBUG("will try to parse : " << data
                                            << ", result : " << has_result);
    return std::make_pair(begin, has_result);
}

class some_result_visitor : public boost::static_visitor<redis_result_t> {
  public:
    template <typename T> redis_result_t operator()(const T &value) const {
        return redis_result_t(value);
    }

    redis_result_t operator()(const protocol_error_t &v) const {
        assert(false &&
               "redis protocol error isn't convertable to redis_result_t");
    }
};

} // namespace bredis
