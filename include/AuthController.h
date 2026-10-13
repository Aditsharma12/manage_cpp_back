#pragma once

#include <drogon/HttpController.h>

class AuthController : public drogon::HttpController<AuthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::registerUser, "/api/auth/register", drogon::Post);
    ADD_METHOD_TO(AuthController::login, "/api/auth/login", drogon::Post);
    ADD_METHOD_TO(AuthController::getProfile, "/api/users/profile", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(AuthController::updateProfile, "/api/users/profile", drogon::Put, "JwtAuthFilter");
    ADD_METHOD_TO(AuthController::adminAddUser, "/api/admin/users", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(AuthController::adminDeleteUser, "/api/admin/users/{1}", drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void registerUser(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void login(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getProfile(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void updateProfile(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void adminAddUser(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void adminDeleteUser(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                         const std::string& userId);
};
