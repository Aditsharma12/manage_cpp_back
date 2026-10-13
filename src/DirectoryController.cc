#include "DirectoryController.h"
#include "DbService.h"
#include <map>

void DirectoryController::getDirectory(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    (void)req;
    auto dbClient = DbService::getClient();

    dbClient->execSqlAsync(
        "SELECT * FROM users ORDER BY id ASC",
        [callback, dbClient](const drogon::orm::Result& usersResult) {
            dbClient->execSqlAsync(
                "SELECT user_id, group_id FROM group_members",
                [callback, usersResult](const drogon::orm::Result& gmResult) {
                    std::map<int, std::vector<std::string>> gmMap;
                    for (const auto& row : gmResult) {
                        gmMap[row["user_id"].as<int>()].push_back(std::to_string(row["group_id"].as<int>()));
                    }

                    Json::Value list(Json::arrayValue);
                    for (const auto& row : usersResult) {
                        int uid = row["id"].as<int>();
                        list.append(DbService::formatUser(row, gmMap[uid]));
                    }

                    auto resp = drogon::HttpResponse::newHttpJsonResponse(list);
                    callback(resp);
                },
                [callback, usersResult](const drogon::orm::DrogonDbException&) {
                    Json::Value list(Json::arrayValue);
                    for (const auto& row : usersResult) {
                        list.append(DbService::formatUser(row));
                    }
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(list);
                    callback(resp);
                }
            );
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
        }
    );
}
