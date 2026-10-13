#include "DbService.h"
#include <drogon/drogon.h>
#include <sstream>

drogon::orm::DbClientPtr DbService::s_client = nullptr;

void DbService::init(const std::string& connInfo, size_t connNum) {
    s_client = drogon::orm::DbClient::newPgClient(connInfo, connNum);
}

drogon::orm::DbClientPtr DbService::getClient() {
    if (s_client) return s_client;
    return drogon::app().getDbClient();
}

bool DbService::parseJsonBody(const drogon::HttpRequestPtr& req, Json::Value& root) {
    if (auto j = req->getJsonObject()) {
        root = *j;
        return true;
    }
    std::string body = std::string(req->body());
    if (body.empty()) return false;
    Json::CharReaderBuilder reader;
    std::string errs;
    std::istringstream s(body);
    return Json::parseFromStream(reader, s, &root, &errs);
}

Json::Value DbService::formatUser(const drogon::orm::Row& row, const std::vector<std::string>& groupIds) {
    Json::Value u;
    u["id"] = std::to_string(row["id"].as<int>());
    u["name"] = row["name"].isNull() ? "" : row["name"].as<std::string>();
    u["roll_number"] = row["roll_number"].isNull() ? "" : row["roll_number"].as<std::string>();
    u["email"] = row["email"].isNull() ? "" : row["email"].as<std::string>();
    u["phone_number"] = row["phone_number"].isNull() ? "" : row["phone_number"].as<std::string>();
    u["about"] = row["about"].isNull() ? "" : row["about"].as<std::string>();

    if (!row["last_login"].isNull()) {
        try {
            u["last_login"] = row["last_login"].as<std::string>();
        } catch (...) {
            u["last_login"] = "";
        }
    }

    Json::Value gArr(Json::arrayValue);
    for (const auto& g : groupIds) {
        gArr.append(g);
    }
    u["group_ids"] = gArr;

    return u;
}

Json::Value DbService::formatGroup(const drogon::orm::Row& row, const std::vector<std::string>& memberIds) {
    Json::Value g;
    g["id"] = std::to_string(row["id"].as<int>());
    g["name"] = row["name"].isNull() ? "" : row["name"].as<std::string>();
    g["purpose"] = row["purpose"].isNull() ? "" : row["purpose"].as<std::string>();
    g["creator_id"] = std::to_string(row["creator_id"].as<int>());
    g["max_capacity"] = row["max_capacity"].as<int>();

    Json::Value mArr(Json::arrayValue);
    for (const auto& m : memberIds) {
        mArr.append(m);
    }
    g["member_ids"] = mArr;

    return g;
}

Json::Value DbService::formatInvite(const drogon::orm::Row& row) {
    Json::Value i;
    i["id"] = std::to_string(row["id"].as<int>());
    i["group_id"] = std::to_string(row["group_id"].as<int>());
    i["sender_id"] = std::to_string(row["sender_id"].as<int>());
    i["receiver_id"] = std::to_string(row["receiver_id"].as<int>());
    i["status"] = row["status"].isNull() ? "pending" : row["status"].as<std::string>();
    if (!row["timestamp"].isNull()) {
        try {
            i["timestamp"] = row["timestamp"].as<std::string>();
        } catch (...) {
            i["timestamp"] = "";
        }
    }
    i["group_name"] = row["group_name"].isNull() ? "" : row["group_name"].as<std::string>();

    return i;
}
