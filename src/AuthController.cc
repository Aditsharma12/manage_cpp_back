#include "AuthController.h"
#include "DbService.h"
#include "SecurityUtils.h"
#include <drogon/drogon.h>

void AuthController::registerUser(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    Json::Value json;
    if (!DbService::parseJsonBody(req, json)) {
        Json::Value err;
        err["error"] = "Invalid JSON body";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string phoneNumber = json.get("phone_number", "").asString();
    if (phoneNumber.empty()) {
        Json::Value err;
        err["error"] = "Phone number is mandatory";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string email = json.get("email", "").asString();
    std::string name = json.get("name", "").asString();
    std::string roll = json.get("roll_number", "").asString();
    std::string password = json.get("password", "").asString();
    std::string passwordHash = security::hashPassword(password);
    std::string role = json.get("role", "student").asString();
    if (role != "admin" && role != "student") {
        role = "student";
    }

    if (role == "admin") {
        std::string secret = json.get("admin_secret", "").asString();
        if (secret != "ADMIN2026") {
            Json::Value err;
            err["error"] = "Invalid Admin secret passcode (Default: ADMIN2026)";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k400BadRequest);
            callback(resp);
            return;
        }
    }

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE email = $1",
        [callback, dbClient, name, roll, email, phoneNumber, passwordHash, role](const drogon::orm::Result& r) {
            if (r.size() > 0) {
                Json::Value err;
                err["error"] = "Email already exists";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }

            dbClient->execSqlAsync(
                "INSERT INTO users (name, roll_number, email, phone_number, password_hash, about, role) "
                "VALUES ($1, $2, $3, $4, $5, 'Hey there! I am using Class Manager.', $6) RETURNING id",
                [callback](const drogon::orm::Result& insertResult) {
                    int newId = insertResult[0]["id"].as<int>();
                    Json::Value res;
                    res["message"] = "Registered";
                    res["user_id"] = std::to_string(newId);
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
                name, roll, email, phoneNumber, passwordHash, role
            );
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
        },
        email
    );
}

void AuthController::login(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    Json::Value json;
    if (!DbService::parseJsonBody(req, json)) {
        Json::Value err;
        err["error"] = "Invalid JSON body";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string email = json.get("email", "").asString();
    std::string password = json.get("password", "").asString();

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT * FROM users WHERE email = $1",
        [callback, dbClient, password](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "Invalid credentials";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k401Unauthorized);
                callback(resp);
                return;
            }

            auto userRow = r[0];
            std::string storedHash = userRow["password_hash"].as<std::string>();
            if (!security::verifyPassword(password, storedHash)) {
                Json::Value err;
                err["error"] = "Invalid credentials";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k401Unauthorized);
                callback(resp);
                return;
            }

            int userId = userRow["id"].as<int>();
            std::string userIdStr = std::to_string(userId);

            // Update last_login
            dbClient->execSqlAsync(
                "UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = $1",
                [](const drogon::orm::Result&) {},
                [](const drogon::orm::DrogonDbException&) {},
                userId
            );

            // Get groups
            dbClient->execSqlAsync(
                "SELECT group_id FROM group_members WHERE user_id = $1",
                [callback, userRow, userIdStr](const drogon::orm::Result& gr) {
                    std::vector<std::string> groupIds;
                    for (const auto& row : gr) {
                        groupIds.push_back(std::to_string(row["group_id"].as<int>()));
                    }

                    std::string jwtSecret;
                    if (const char* envSecret = std::getenv("JWT_SECRET_KEY")) {
                        jwtSecret = envSecret;
                    } else {
                        auto customConfig = drogon::app().getCustomConfig();
                        if (customConfig.isMember("jwt_secret")) {
                            jwtSecret = customConfig["jwt_secret"].asString();
                        }
                    }
                    if (jwtSecret.empty()) jwtSecret = "super-secret-fallback-key";
                    std::string token = security::createJwtToken(userIdStr, jwtSecret);

                    Json::Value res;
                    res["access_token"] = token;
                    res["user"] = DbService::formatUser(userRow, groupIds);

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
                userId
            );
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
        },
        email
    );
}

void AuthController::getProfile(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string userIdStr = req->getAttributes()->get<std::string>("user_id");
    int userId = 0;
    try {
        userId = std::stoi(userIdStr);
    } catch (...) {
        Json::Value err;
        err["error"] = "Invalid user ID";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "SELECT * FROM users WHERE id = $1",
        [callback, dbClient, userId](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "User not found";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }

            auto userRow = r[0];
            dbClient->execSqlAsync(
                "SELECT group_id FROM group_members WHERE user_id = $1",
                [callback, userRow](const drogon::orm::Result& gr) {
                    std::vector<std::string> groupIds;
                    for (const auto& row : gr) {
                        groupIds.push_back(std::to_string(row["group_id"].as<int>()));
                    }
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(DbService::formatUser(userRow, groupIds));
                    callback(resp);
                },
                [callback, userRow](const drogon::orm::DrogonDbException&) {
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(DbService::formatUser(userRow));
                    callback(resp);
                },
                userId
            );
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
        },
        userId
    );
}

void AuthController::updateProfile(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string userIdStr = req->getAttributes()->get<std::string>("user_id");
    int userId = std::stoi(userIdStr);

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
    std::string roll = json.get("roll_number", "").asString();
    std::string phone = json.get("phone_number", "").asString();
    std::string about = json.get("about", "").asString();

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "UPDATE users SET name = $1, roll_number = $2, phone_number = $3, about = $4 WHERE id = $5 RETURNING *",
        [callback, dbClient, userId](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "User not found";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }

            auto userRow = r[0];
            dbClient->execSqlAsync(
                "SELECT group_id FROM group_members WHERE user_id = $1",
                [callback, userRow](const drogon::orm::Result& gr) {
                    std::vector<std::string> groupIds;
                    for (const auto& row : gr) {
                        groupIds.push_back(std::to_string(row["group_id"].as<int>()));
                    }
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(DbService::formatUser(userRow, groupIds));
                    callback(resp);
                },
                [callback, userRow](const drogon::orm::DrogonDbException&) {
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(DbService::formatUser(userRow));
                    callback(resp);
                },
                userId
            );
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
        },
        name, roll, phone, about, userId
    );
}

void AuthController::adminAddUser(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    Json::Value json;
    if (!DbService::parseJsonBody(req, json)) {
        Json::Value err;
        err["error"] = "Invalid JSON body";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string email = json.get("email", "").asString();
    std::string name = json.get("name", "").asString();
    std::string roll = json.get("roll_number", "").asString();
    std::string phone = json.get("phone_number", "N/A").asString();
    std::string role = json.get("role", "student").asString();
    std::string password = json.get("password", "password123").asString();
    std::string passwordHash = security::hashPassword(password);

    if (email.empty() || name.empty()) {
        Json::Value err;
        err["error"] = "Name and email are required";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "INSERT INTO users (name, roll_number, email, phone_number, password_hash, about, role) "
        "VALUES ($1, $2, $3, $4, $5, 'Student record added by administrator.', $6) RETURNING *",
        [callback](const drogon::orm::Result& r) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(DbService::formatUser(r[0]));
            resp->setStatusCode(drogon::k201Created);
            callback(resp);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k400BadRequest);
            callback(resp);
        },
        name, roll, email, phone, passwordHash, role
    );
}

void AuthController::adminDeleteUser(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                     const std::string& userIdStr) {
    (void)req;
    int userId = 0;
    try {
        userId = std::stoi(userIdStr);
    } catch (...) {
        Json::Value err;
        err["error"] = "Invalid user ID";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = DbService::getClient();
    dbClient->execSqlAsync(
        "DELETE FROM users WHERE id = $1 RETURNING id",
        [callback](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                Json::Value err;
                err["error"] = "Student not found";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }
            Json::Value res;
            res["message"] = "Student deleted successfully";
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
        userId
    );
}
