//
// Created by boa on 13.05.19.
//

#ifndef BASIC_CONTROLLER_HPP
#define BASIC_CONTROLLER_HPP


#include <string>
#include <cpprest/http_listener.h>
#include <pplx/pplxtasks.h>
#include "controller.hpp"

using namespace web;
using namespace http::experimental::listener;

namespace cfx {
    class BasicController {
    protected:
        http_listener _listener; // main micro service network endpoint

    public:
        BasicController();
        ~BasicController();

        void setEndpoint(const std::string & value);
        std::string endpoint() const;
        pplx::task<void> accept();
        pplx::task<void> shutdown();

        virtual void initRestOpHandlers() {
            /* had to be implemented by the child class */
        }

        std::vector<utility::string_t> requestPath(const http_request & message);
    };
}

#endif //BASIC_CONTROLLER_HPP
