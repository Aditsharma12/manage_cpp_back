#include "InviteController.h"
#include "DbService.h"

void InviteController::sendInvite(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string currentUserIdStr = req->getAttributes()->get<std::string>("user_id");
    int currentUserId = std::stoi(currentUserIdStr);

    Json::Value json;
    if (!DbService::parseJsonBody(req, json)) {
        Json::Value err;
        err["error"] = "Invalid JSON body";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    int receiverId = std::stoi(json.get("receiver_id", "0").asString());
    int groupId = std::stoi(json.get("group_id", "0").asString());

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT * FROM groups WHERE id = $1",
        [callback, dbClient, currentUserId, receiverId, groupId](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "Group not found";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }

            std::string groupName = r[0]["name"].as<std::string>();

            // Check if user already in group
            dbClient->execSqlAsync(
                "SELECT 1 FROM group_members WHERE group_id = $1 AND user_id = $2",
                [callback, dbClient, currentUserId, receiverId, groupId, groupName](const drogon::orm::Result& mr) {
                    if (mr.size() > 0) {
                        Json::Value err;
                        err["error"] = "User already in group";
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                        resp->setStatusCode(drogon::k400BadRequest);
                        callback(resp);
                        return;
                    }

                    // Check if invite pending
                    dbClient->execSqlAsync(
                        "SELECT 1 FROM invites WHERE group_id = $1 AND receiver_id = $2 AND status = 'pending'",
                        [callback, dbClient, currentUserId, receiverId, groupId, groupName](const drogon::orm::Result& ir) {
                            if (ir.size() > 0) {
                                Json::Value err;
                                err["error"] = "Invite already pending";
                                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                                resp->setStatusCode(drogon::k400BadRequest);
                                callback(resp);
                                return;
                            }

                            // Insert invite
                            dbClient->execSqlAsync(
                                "INSERT INTO invites (group_id, sender_id, receiver_id, status, group_name) "
                                "VALUES ($1, $2, $3, 'pending', $4) RETURNING id",
                                [callback](const drogon::orm::Result&) {
                                    Json::Value res;
                                    res["message"] = "Invite sent";
                                    auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                                    resp->setStatusCode(drogon::k201Created);
                                    callback(resp);
                                },
                                [callback](const drogon::orm::DrogonDbException& e) {
                                    Json::Value err;
                                    err["error"] = e.base().what();
                                    auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                                    resp->setStatusCode(drogon::k500InternalServerError);
                                    callback(resp);
                                },
                                groupId, currentUserId, receiverId, groupName
                            );
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            Json::Value err;
                            err["error"] = e.base().what();
                            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                            resp->setStatusCode(drogon::k500InternalServerError);
                            callback(resp);
                        },
                        groupId, receiverId
                    );
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    Json::Value err;
                    err["error"] = e.base().what();
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                    resp->setStatusCode(drogon::k500InternalServerError);
                    callback(resp);
                },
                groupId, receiverId
            );
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
        },
        groupId
    );
}

void InviteController::getPendingInvites(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string currentUserIdStr = req->getAttributes()->get<std::string>("user_id");
    int currentUserId = std::stoi(currentUserIdStr);

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT i.*, u.name AS sender_name, u.email AS sender_email, u.roll_number AS sender_roll "
        "FROM invites i "
        "LEFT JOIN users u ON i.sender_id = u.id "
        "WHERE i.receiver_id = $1 AND i.status = 'pending' "
        "ORDER BY i.timestamp DESC",
        [callback](const drogon::orm::Result& r) {
            Json::Value list(Json::arrayValue);
            for (const auto& row : r) {
                list.append(DbService::formatInvite(row));
            }
            auto resp = drogon::HttpResponse::newHttpJsonResponse(list);
            callback(resp);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
        },
        currentUserId
    );
}

void InviteController::respondInvite(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                     const std::string& inviteIdStr) {
    std::string currentUserIdStr = req->getAttributes()->get<std::string>("user_id");
    int currentUserId = std::stoi(currentUserIdStr);
    int inviteId = std::stoi(inviteIdStr);

    Json::Value json;
    if (!DbService::parseJsonBody(req, json)) {
        Json::Value err;
        err["error"] = "Invalid JSON body";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string action = json.get("action", "").asString();

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT * FROM invites WHERE id = $1 AND receiver_id = $2",
        [callback, dbClient, currentUserId, inviteId, action](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "Not found";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }

            int groupId = r[0]["group_id"].as<int>();

            if (action == "accept") {
                dbClient->execSqlAsync(
                    "INSERT INTO group_members (group_id, user_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
                    [callback, dbClient, inviteId](const drogon::orm::Result&) {
                        dbClient->execSqlAsync(
                            "UPDATE invites SET status = 'accepted' WHERE id = $1",
                            [callback](const drogon::orm::Result&) {
                                Json::Value res;
                                res["message"] = "Accepted!";
                                auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                                callback(resp);
                            },
                            [callback](const drogon::orm::DrogonDbException& e) {
                                Json::Value err;
                                err["error"] = e.base().what();
                                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                                resp->setStatusCode(drogon::k500InternalServerError);
                                callback(resp);
                            },
                            inviteId
                        );
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        Json::Value err;
                        err["error"] = e.base().what();
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                        resp->setStatusCode(drogon::k500InternalServerError);
                        callback(resp);
                    },
                    groupId, currentUserId
                );
            } else if (action == "decline") {
                dbClient->execSqlAsync(
                    "UPDATE invites SET status = 'declined' WHERE id = $1",
                    [callback](const drogon::orm::Result&) {
                        Json::Value res;
                        res["message"] = "Declined.";
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                        callback(resp);
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        Json::Value err;
                        err["error"] = e.base().what();
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                        resp->setStatusCode(drogon::k500InternalServerError);
                        callback(resp);
                    },
                    inviteId
                );
            } else {
                Json::Value err;
                err["error"] = "Invalid action";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
            }
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
        },
        inviteId, currentUserId
    );
}
