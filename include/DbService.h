#pragma once

#include <drogon/orm/DbClient.h>
#include <drogon/HttpRequest.h>
#include <json/json.h>
#include <string>
#include <vector>

class DbService {
public:
    static void init(const std::string& connInfo, size_t connNum = 5);
    static drogon::orm::DbClientPtr getClient();
    static bool parseJsonBody(const drogon::HttpRequestPtr& req, Json::Value& root);

    // Helper formatting functions matching API contract
    static Json::Value formatUser(const drogon::orm::Row& row, const std::vector<std::string>& groupIds = {});
    static Json::Value formatGroup(const drogon::orm::Row& row, const std::vector<std::string>& memberIds = {});
    static Json::Value formatInvite(const drogon::orm::Row& row);

private:
    static drogon::orm::DbClientPtr s_client;
};
