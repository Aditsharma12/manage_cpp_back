#pragma once

#include <drogon/HttpController.h>

class GroupController : public drogon::HttpController<GroupController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(GroupController::createGroup, "/api/groups", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(GroupController::myGroups, "/api/groups/my_groups", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(GroupController::getGroupDetails, "/api/groups/{1}", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(GroupController::updateGroup, "/api/groups/{1}", drogon::Put, "JwtAuthFilter");
    ADD_METHOD_TO(GroupController::getAllGroups, "/api/admin/groups", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(GroupController::getAllGroups, "/api/groups", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(GroupController::adminDeleteGroup, "/api/admin/groups/{1}", drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void createGroup(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void myGroups(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getAllGroups(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getGroupDetails(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                         const std::string& groupId);

    void updateGroup(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& groupId);

    void deleteOrLeaveGroup(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& groupId);

    void adminDeleteGroup(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          const std::string& groupId);
};
