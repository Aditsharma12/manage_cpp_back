#include "GroupController.h"
#include "DbService.h"
#include <map>

void GroupController::createGroup(const drogon::HttpRequestPtr& req,
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

    std::string name = json.get("name", "").asString();
    std::string purpose = json.get("purpose", "").asString();
    std::string section = json.get("section", "General").asString();
    std::string activityName = json.get("activity_name", "").asString();
    int maxCapacity = json.get("max_capacity", 5).asInt();

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT role FROM users WHERE id = $1",
        [callback, dbClient, currentUserId, currentUserIdStr, name, purpose, section, activityName, maxCapacity](const drogon::orm::Result& userR) {
            bool isAdmin = false;
            if (userR.size() > 0 && !userR[0]["role"].isNull()) {
                isAdmin = (userR[0]["role"].as<std::string>() == "admin");
            }

            dbClient->execSqlAsync(
                "INSERT INTO groups (name, purpose, creator_id, max_capacity, section, activity_name) VALUES ($1, $2, $3, $4, $5, $6) RETURNING *",
                [callback, dbClient, currentUserId, currentUserIdStr, isAdmin](const drogon::orm::Result& r) {
                    auto groupRow = r[0];
                    int groupId = groupRow["id"].as<int>();

                    if (isAdmin) {
                        // Admin is creator/coordinator only, NOT a member of student group
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(
                            DbService::formatGroup(groupRow, {})
                        );
                        resp->setStatusCode(drogon::k201Created);
                        callback(resp);
                    } else {
                        // Student is enrolled as initial member
                        dbClient->execSqlAsync(
                            "INSERT INTO group_members (group_id, user_id) VALUES ($1, $2)",
                            [callback, groupRow, currentUserIdStr](const drogon::orm::Result&) {
                                auto resp = drogon::HttpResponse::newHttpJsonResponse(
                                    DbService::formatGroup(groupRow, {currentUserIdStr})
                                );
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
                            groupId, currentUserId
                        );
                    }
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    Json::Value err;
                    err["error"] = e.base().what();
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                    resp->setStatusCode(drogon::k500InternalServerError);
                    callback(resp);
                },
                name, purpose, currentUserId, maxCapacity, section, activityName
            );
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

void GroupController::myGroups(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string currentUserIdStr = req->getAttributes()->get<std::string>("user_id");
    int currentUserId = std::stoi(currentUserIdStr);

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT g.* FROM groups g JOIN group_members gm ON g.id = gm.group_id WHERE gm.user_id = $1",
        [callback, dbClient](const drogon::orm::Result& groupsResult) {
            if (groupsResult.size() == 0) {
                Json::Value list(Json::arrayValue);
                auto resp = drogon::HttpResponse::newHttpJsonResponse(list);
                callback(resp);
                return;
            }

            dbClient->execSqlAsync(
                "SELECT group_id, user_id FROM group_members",
                [callback, groupsResult](const drogon::orm::Result& gmResult) {
                    std::map<int, std::vector<std::string>> gmMap;
                    for (const auto& row : gmResult) {
                        gmMap[row["group_id"].as<int>()].push_back(std::to_string(row["user_id"].as<int>()));
                    }

                    Json::Value list(Json::arrayValue);
                    for (const auto& row : groupsResult) {
                        int gid = row["id"].as<int>();
                        list.append(DbService::formatGroup(row, gmMap[gid]));
                    }

                    auto resp = drogon::HttpResponse::newHttpJsonResponse(list);
                    callback(resp);
                },
                [callback, groupsResult](const drogon::orm::DrogonDbException&) {
                    Json::Value list(Json::arrayValue);
                    for (const auto& row : groupsResult) {
                        list.append(DbService::formatGroup(row));
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
        },
        currentUserId
    );
}

void GroupController::getAllGroups(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    (void)req;
    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT g.*, u.name AS creator_name, u.email AS creator_email "
        "FROM groups g "
        "LEFT JOIN users u ON g.creator_id = u.id "
        "ORDER BY g.id DESC",
        [callback, dbClient](const drogon::orm::Result& groupsResult) {
            if (groupsResult.size() == 0) {
                Json::Value list(Json::arrayValue);
                auto resp = drogon::HttpResponse::newHttpJsonResponse(list);
                callback(resp);
                return;
            }

            dbClient->execSqlAsync(
                "SELECT gm.group_id, u.id, u.name, u.roll_number, u.email, u.phone_number "
                "FROM group_members gm "
                "JOIN users u ON gm.user_id = u.id",
                [callback, groupsResult](const drogon::orm::Result& gmResult) {
                    std::map<int, std::vector<std::string>> gmMap;
                    std::map<int, Json::Value> membersMap;
                    for (const auto& row : gmResult) {
                        int gid = row["group_id"].as<int>();
                        gmMap[gid].push_back(std::to_string(row["id"].as<int>()));

                        Json::Value m;
                        m["id"] = std::to_string(row["id"].as<int>());
                        m["name"] = row["name"].isNull() ? "" : row["name"].as<std::string>();
                        m["roll_number"] = row["roll_number"].isNull() ? "" : row["roll_number"].as<std::string>();
                        m["email"] = row["email"].isNull() ? "" : row["email"].as<std::string>();
                        m["phone_number"] = row["phone_number"].isNull() ? "" : row["phone_number"].as<std::string>();
                        membersMap[gid].append(m);
                    }

                    Json::Value list(Json::arrayValue);
                    for (const auto& row : groupsResult) {
                        int gid = row["id"].as<int>();
                        Json::Value g = DbService::formatGroup(row, gmMap[gid]);
                        g["members"] = membersMap.count(gid) ? membersMap[gid] : Json::Value(Json::arrayValue);
                        list.append(g);
                    }

                    auto resp = drogon::HttpResponse::newHttpJsonResponse(list);
                    callback(resp);
                },
                [callback, groupsResult](const drogon::orm::DrogonDbException&) {
                    Json::Value list(Json::arrayValue);
                    for (const auto& row : groupsResult) {
                        list.append(DbService::formatGroup(row));
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

void GroupController::adminDeleteGroup(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                       const std::string& groupIdStr) {
    (void)req;
    int groupId = 0;
    try {
        groupId = std::stoi(groupIdStr);
    } catch (...) {
        Json::Value err;
        err["error"] = "Invalid group ID";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "DELETE FROM groups WHERE id = $1 RETURNING id",
        [callback](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "Group not found";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }
            Json::Value res;
            res["message"] = "Group deleted by Admin";
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
        groupId
    );
}

void GroupController::getGroupDetails(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                      const std::string& groupIdStr) {
    (void)req;
    int groupId = std::stoi(groupIdStr);
    auto dbClient = DbService::getClient();

    dbClient->execSqlAsync(
        "SELECT * FROM groups WHERE id = $1",
        [callback, dbClient, groupId](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "Not found";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }

            auto groupRow = r[0];

            dbClient->execSqlAsync(
                "SELECT u.* FROM users u JOIN group_members gm ON u.id = gm.user_id WHERE gm.group_id = $1",
                [callback, groupRow](const drogon::orm::Result& membersResult) {
                    std::vector<std::string> memberIds;
                    Json::Value membersArray(Json::arrayValue);

                    for (const auto& mRow : membersResult) {
                        memberIds.push_back(std::to_string(mRow["id"].as<int>()));
                        membersArray.append(DbService::formatUser(mRow));
                    }

                    Json::Value formattedGroup = DbService::formatGroup(groupRow, memberIds);
                    formattedGroup["members"] = membersArray;

                    auto resp = drogon::HttpResponse::newHttpJsonResponse(formattedGroup);
                    callback(resp);
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

void GroupController::updateGroup(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                  const std::string& groupIdStr) {
    std::string currentUserIdStr = req->getAttributes()->get<std::string>("user_id");
    int currentUserId = std::stoi(currentUserIdStr);
    int groupId = std::stoi(groupIdStr);

    Json::Value json;
    if (!DbService::parseJsonBody(req, json)) {
        Json::Value err;
        err["error"] = "Invalid JSON body";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string name = json.get("name", "").asString();
    std::string purpose = json.get("purpose", "").asString();

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT * FROM groups WHERE id = $1",
        [callback, dbClient, currentUserId, groupId, name, purpose](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "Not found";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }

            int creatorId = r[0]["creator_id"].as<int>();
            if (creatorId != currentUserId) {
                Json::Value err;
                err["error"] = "Only the creator can edit this group";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k403Forbidden);
                callback(resp);
                return;
            }

            dbClient->execSqlAsync(
                "UPDATE groups SET name = $1, purpose = $2 WHERE id = $3",
                [callback](const drogon::orm::Result&) {
                    Json::Value res;
                    res["message"] = "Group updated";
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
                name, purpose, groupId
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

void GroupController::deleteOrLeaveGroup(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                        const std::string& groupIdStr) {
    std::string currentUserIdStr = req->getAttributes()->get<std::string>("user_id");
    int currentUserId = std::stoi(currentUserIdStr);
    int groupId = std::stoi(groupIdStr);

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT * FROM groups WHERE id = $1",
        [callback, dbClient, currentUserId, groupId](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "Not found";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }

            int creatorId = r[0]["creator_id"].as<int>();

            if (creatorId == currentUserId) {
                dbClient->execSqlAsync(
                    "DELETE FROM groups WHERE id = $1",
                    [callback](const drogon::orm::Result&) {
                        Json::Value res;
                        res["message"] = "Group deleted permanently";
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
                    groupId
                );
            } else {
                dbClient->execSqlAsync(
                    "DELETE FROM group_members WHERE group_id = $1 AND user_id = $2",
                    [callback](const drogon::orm::Result&) {
                        Json::Value res;
                        res["message"] = "You left the group";
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
                    groupId, currentUserId
                );
            }
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
