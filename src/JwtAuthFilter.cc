#include "JwtAuthFilter.h"
#include "SecurityUtils.h"
#include <drogon/drogon.h>
#include <cstdlib>

void JwtAuthFilter::doFilter(const drogon::HttpRequestPtr& req,
                             drogon::FilterCallback&& fcb,
                             drogon::FilterChainCallback&& fccb) {
    std::string authHeader = req->getHeader("Authorization");
    if (authHeader.rfind("Bearer ", 0) != 0) {
        Json::Value err;
        err["error"] = "Authorization header missing or invalid format (Bearer token expected)";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
        return;
    }

    std::string token = authHeader.substr(7);
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

    std::string userId;
    if (!security::verifyJwtToken(token, jwtSecret, userId)) {
        Json::Value err;
        err["error"] = "Invalid or expired token";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
        return;
    }

    req->getAttributes()->insert("user_id", userId);
    fccb();
}
