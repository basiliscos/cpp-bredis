#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <iostream>

#include <boost/process/child.hpp>

namespace test_server {
struct TestServer {
    std::unique_ptr<boost::process::child> child;

    TestServer(std::initializer_list<std::string> &&args) {

        auto it = args.begin();
        std::string stringized;
        for (auto i = 0; i < args.size(); i++, it++) {
            const char *c_arg = (*it).c_str();
            stringized += " ";
            stringized += c_arg;
        }
        std::cout << "going to fork to start: " << stringized << std::endl;

        child = std::make_unique<boost::process::child>(stringized);
    }
    ~TestServer() {
        std::cout << "terminating child " << child->id() << "\n";
    }
};

using result_t = std::unique_ptr<TestServer>;

result_t make_server(std::initializer_list<std::string> &&args) {
    return std::make_unique<TestServer>(std::move(args));
}
};
