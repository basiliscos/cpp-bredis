#pragma once
/* platform-specific headers */

#if defined(_WIN32) || defined(WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define SOCKET_TYPE SOCKET
#define CLOSE_SOCKET(s) closesocket(s)
#pragma comment(lib, "Ws2_32.lib")
#else
#define SOCKET_TYPE int
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
#include <unistd.h> /* Needed for close() */
#define CLOSE_SOCKET(s) close(s)
#endif

#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <random>
#include <chrono>
#include <thread>

namespace empty_port {

enum Kind { TCP, UDP };

using socket_t = SOCKET_TYPE;
using port_t = uint16_t;
using clock_t = std::chrono::high_resolution_clock;

class SocketHolder {
    socket_t socket_;

  public:
    SocketHolder(socket_t s) : socket_(s) {
        if (s == -1) {
            throw std::runtime_error("Cannot get socket");
        }
    }
    ~SocketHolder() { CLOSE_SOCKET(socket_); }
    socket_t &socket() { return socket_; }
};

// using duration_t = std::chrono::milliseconds;
// static constexpr duration_t SLEEP_DELAY(10);

static constexpr port_t MIN_PORT = 49152;
static constexpr port_t MAX_PORT = 65535;

template <Kind> struct impl;

template <> struct impl<TCP> {

    static bool can_listen(const port_t port, const char *host);

    static void fill_struct(const socket_t &socket, sockaddr_in &addr,
                            const port_t port, const char *host);

    static bool check_port_impl(const port_t port, const char *host);

    static port_t get_random_impl(const char *host);

    template <typename D>
    static bool wait_port_impl(const port_t port, const char *host, D max_wait);
};

/* TCP impl */
template <typename D>
inline bool impl<TCP>::wait_port_impl(const port_t port, const char *host,
                                      D max_wait) {
    auto stop_at = clock_t::now() + max_wait;
    do {
        if (!impl<TCP>::check_port_impl(port, host)) {
            return true;
        }
        std::this_thread::sleep_for(D(1));
    } while (clock_t::now() < stop_at);

    return false;
}

inline port_t impl<TCP>::get_random_impl(const char *host) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<port_t> uni(MIN_PORT, MAX_PORT);

    port_t some_port = uni(rng);
    while (some_port < MAX_PORT) {
        bool is_empty = impl<TCP>::check_port_impl(some_port, host) &&
                        impl<TCP>::can_listen(some_port, host);
        if (is_empty) {
            return some_port;
        }
        some_port++;
    }
    throw std::runtime_error("Cannot get random port");
}

inline void impl<TCP>::fill_struct(const socket_t & /*socket*/, sockaddr_in &addr,
                                   const port_t port, const char *host) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    struct addrinfo hints, *result = NULL;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct sockaddr_in *host_addr;
    // looks up IPv4/IPv6 address by host name or stringized IP address
    int r = getaddrinfo(host, NULL, &hints, &result);
    if (r) {
        throw std::runtime_error(std::string("Cannot getaddrinfo:: ") +
                                 strerror(r));
    }
    host_addr = (struct sockaddr_in *)result->ai_addr;
    memcpy(&addr.sin_addr, &host_addr->sin_addr, sizeof(struct in_addr));
    freeaddrinfo(result);
}

inline bool impl<TCP>::can_listen(const port_t port, const char *host) {
    SocketHolder sh(socket(AF_INET, SOCK_STREAM, 0));
    sockaddr_in addr;
    fill_struct(sh.socket(), addr, port, host);

    if (bind(sh.socket(), (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        return false;
    }
    if (listen(sh.socket(), 1)) {
        return false;
    }

    /* success */
    return true;
}

inline bool impl<TCP>::check_port_impl(const port_t port, const char *host) {
    SocketHolder sh(socket(AF_INET, SOCK_STREAM, 0));
    sockaddr_in remote_addr;

    fill_struct(sh.socket(), remote_addr, port, host);

    if ((connect(sh.socket(), (struct sockaddr *)&remote_addr,
                 sizeof(remote_addr)))) {
        return true;
    } else {
        return false;
    }
}

/* public interface */
template <Kind T = Kind::TCP>
inline bool check_port(const port_t port, const char *host = "127.0.0.1") {
    return impl<T>::check_port_impl(port, host);
}

template <Kind T = Kind::TCP>
inline port_t get_random(const char *host = "127.0.0.1") {
    return impl<T>::get_random_impl(host);
}

template <Kind T = Kind::TCP, typename D = std::chrono::milliseconds>
inline bool wait_port(const port_t port, const char *host = "127.0.0.1",
                      D max_wait = D(500)) {
    return impl<T>::template wait_port_impl<D>(port, host, max_wait);
}
}
