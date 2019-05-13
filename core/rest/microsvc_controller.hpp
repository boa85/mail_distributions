//
// Created by boa on 13.05.19.
//

#ifndef MICROSVC_CONTROLLER_HPP
#define MICROSVC_CONTROLLER_HPP


#include "foundation/include/basic_controller.hpp"
#include "foundation/include/controller.hpp"

using namespace cfx;

class MicroserviceController : public BasicController, Controller {
public:
    MicroserviceController() : BasicController() {}
    virtual ~MicroserviceController()  = default;
    void handleGet(http_request message) override;
    void handlePut(http_request message) override;
    void handlePost(http_request message) override;
    void handlePatch(http_request message) override;
    void handleDelete(http_request message) override;
    void handleHead(http_request message) override;
    void handleOptions(http_request message) override;
    void handleTrace(http_request message) override;
    void handleConnect(http_request message) override;
    void handleMerge(http_request message) override;
    void initRestOpHandlers() override;

private:
    static json::value responseNotImpl(const http::method & method);
};

#endif //CONTROLLER_HPP
