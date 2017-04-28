#pragma once

#include <boost/asio/buffers_iterator.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <utility>

namespace bredis {
namespace test {

template <typename NextLayer> class SocketWithLogging {
    NextLayer stream_;

  public:
    template <typename... Args>
    SocketWithLogging(Args &&... args) : stream_(std::forward<Args>(args)...) {}

    template <typename BufferSequence>
    void dump(const char *prefix, const BufferSequence &buffers) {
        using Iterator = boost::asio::buffers_iterator<BufferSequence, char>;

        Iterator it = Iterator::begin(buffers);
        Iterator end = Iterator::end(buffers);
        auto size = std::distance(it, end);

        std::cout << "{" << prefix << " " << size << " bytes}[";
        for (; it != end; it++) {
            std::cout << *it;
        }
        std::cout << "]" << std::endl;
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read_some(const MutableBufferSequence &buffers,
                                ReadHandler handler) {
        stream_.async_read_some(buffers, handler);
        dump("async_read_some", buffers);
    }

    template <typename ConstBufferSequence, typename WriteHandler>
    void async_write_some(const ConstBufferSequence &buffers,
                                 WriteHandler handler) {
        dump("async_write_some", buffers);
        stream_.async_write_some(buffers, handler);
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence &buffers) {
        dump("write_some", buffers);
        return stream_.write_some(buffers);
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence &buffers,
                           boost::system::error_code &ec) {
        dump("write_some", buffers);
        return stream_.write_some(buffers, ec);
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence &buffers) {
        auto result = stream_.read_some(buffers);
        dump("read_some", buffers);
        return result;
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence &buffers,
                          boost::system::error_code &ec) {
        auto result = stream_.read_some(buffers, ec);
        dump("read_some", buffers);
        return result;
    }
};

} // namespace test
} // namespace bredis
