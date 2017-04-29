#pragma once

#include <boost/asio/buffers_iterator.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <string>
#include <utility>

namespace bredis {
namespace test {

class DefaultLogPolicy {
  public:
    static void log(const char *prefix, const std::string &content) {
        std::cout << "{" << prefix << " " << content.size() << " bytes}["
                  << content << "]" << std::endl;
    }
};

template <typename NextLayer, typename LogPolicy = DefaultLogPolicy>
class SocketWithLogging {
    NextLayer stream_;

    template <typename BufferSequence, typename Handler>
    struct HandlerWithLogging {
        Handler next_handler_;
        const BufferSequence buffers_;
        const char *prefix_;

        HandlerWithLogging(const char *prefix, Handler hander,
                           const BufferSequence &buffers)
            : prefix_(prefix), next_handler_(hander), buffers_(buffers) {}

        void operator()(const boost::system::error_code &error_code,
                        std::size_t bytes_transferred) {
            dump(prefix_, buffers_, bytes_transferred);
            next_handler_(error_code, bytes_transferred);
        }
    };

  public:
    template <typename... Args>
    SocketWithLogging(Args &&... args) : stream_(std::forward<Args>(args)...) {}

    template <typename BufferSequence>
    static void dump(const char *prefix, const BufferSequence &buffers,
                     std::size_t size) {
        using boost::asio::buffer_cast;
        using boost::asio::buffer_size;

        std::string content;
        content.reserve(size);
        for (auto const &buffer : buffers) {
            content.append(buffer_cast<char const *>(buffer),
                           buffer_size(buffer));
        }
        LogPolicy::log(prefix, content);
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read_some(const MutableBufferSequence &buffers,
                         ReadHandler handler) {
        stream_.async_read_some(
            buffers, HandlerWithLogging<MutableBufferSequence, ReadHandler>(
                         "async_read_some", handler, buffers));
    }

    template <typename ConstBufferSequence, typename WriteHandler>
    void async_write_some(const ConstBufferSequence &buffers,
                          WriteHandler handler) {
        stream_.async_write_some(
            buffers, HandlerWithLogging<ConstBufferSequence, WriteHandler>(
                         "async_write_some", handler, buffers));
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence &buffers) {
        auto bytes_transferred = stream_.write_some(buffers);
        dump("write_some", buffers, bytes_transferred);
        return bytes_transferred;
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence &buffers,
                           boost::system::error_code &ec) {
        auto bytes_transferred = stream_.write_some(buffers, ec);
        dump("write_some", buffers, bytes_transferred);
        return bytes_transferred;
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence &buffers) {
        auto bytes_transferred = stream_.read_some(buffers);
        dump("read_some", buffers, bytes_transferred);
        return bytes_transferred;
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence &buffers,
                          boost::system::error_code &ec) {
        auto bytes_transferred = stream_.read_some(buffers, ec);
        dump("read_some", buffers, bytes_transferred);
        return bytes_transferred;
    }
};

} // namespace test
} // namespace bredis
