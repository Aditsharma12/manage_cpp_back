#pragma once

#include <drogon/HttpController.h>

class DirectoryController : public drogon::HttpController<DirectoryController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DirectoryController::getDirectory, "/api/directory", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    void getDirectory(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};
