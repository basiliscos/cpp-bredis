#pragma once

#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#include <boost/algorithm/string/join.hpp>
#include <boost/process/child.hpp>

namespace test_server {
struct TestServer {
    std::unique_ptr<boost::process::child> child;

    TestServer(std::initializer_list<std::string> &&args) {
        std::string stringized = boost::algorithm::join(args, " ");
        std::cout << "going to fork to start: " << stringized << std::endl;
        child = std::make_unique<boost::process::child>(stringized);
    }
    ~TestServer() { std::cout << "terminating child " << child->id() << "\n"; }
};

using result_t = std::unique_ptr<TestServer>;

result_t make_server(std::initializer_list<std::string> &&args) {
    return std::make_unique<TestServer>(std::move(args));
}
}
