#pragma once

#include <drogon/HttpController.h>

class InviteController : public drogon::HttpController<InviteController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(InviteController::sendInvite, "/api/invites", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(InviteController::getPendingInvites, "/api/invites/pending", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(InviteController::respondInvite, "/api/invites/{1}/respond", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void sendInvite(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getPendingInvites(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void respondInvite(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       const std::string& inviteId);
};
