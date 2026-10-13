#include <drogon/drogon.h>
#include "DbService.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

// Lightweight .env loader
void loadEnv(const std::string& path = ".env") {
    std::ifstream file(path);
    if (!file.is_open()) {
        file.open("../" + path);
        if (!file.is_open()) return;
    }
    std::string line;
    while (std::getline(file, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') continue;
        size_t eq = line.find('=', first);
        if (eq == std::string::npos) continue;

        std::string key = line.substr(first, eq - first);
        key.erase(key.find_last_not_of(" \t\r\n") + 1);

        std::string val = line.substr(eq + 1);
        size_t valStart = val.find_first_not_of(" \t\r\n");
        if (valStart != std::string::npos) {
            val = val.substr(valStart);
            val.erase(val.find_last_not_of(" \t\r\n") + 1);
        } else {
            val = "";
        }
        if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || 
                                (val.front() == '\'' && val.back() == '\''))) {
            val = val.substr(1, val.size() - 2);
        }
#ifdef _WIN32
        _putenv_s(key.c_str(), val.c_str());
#else
        setenv(key.c_str(), val.c_str(), 1);
#endif
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Starting backend_fast (Drogon C++ with PostgreSQL)..." << std::endl;

    // 1. Load environment variables from .env
    loadEnv();

    std::string configPath = "config.json";
    if (argc > 1) {
        configPath = argv[1];
    }

    try {
        drogon::app().loadConfigFile(configPath);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not load " << configPath << ": " << e.what() << std::endl;
        std::cerr << "Falling back to default listener 0.0.0.0:5000" << std::endl;
        drogon::app().addListener("0.0.0.0", 5000);
    }

    // Enable Global CORS support matching Flask CORS(app)
    drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr& req,
                                              drogon::FilterCallback&& fcb,
                                              drogon::FilterChainCallback&& fccb) {
        if (req->method() == drogon::HttpMethod::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
            fcb(resp);
            return;
        }
        fccb();
    });

    drogon::app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr& req,
                                                const drogon::HttpResponsePtr& resp) {
        (void)req;
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
    });

    // 2. Resolve Postgres URI strictly from environment (secure)
    std::string pgUri;
    const char* envUri = std::getenv("DATABASE_URL");
    if (!envUri) envUri = std::getenv("POSTGRES_URI");
    if (envUri) {
        pgUri = envUri;
    } else {
        auto customConfig = drogon::app().getCustomConfig();
        if (customConfig.isMember("postgres_uri")) {
            pgUri = customConfig["postgres_uri"].asString();
        }
    }

    if (pgUri.empty()) {
        std::cerr << "[Security Error] No DATABASE_URL or POSTGRES_URI set in .env or environment!" << std::endl;
        return 1;
    }

    try {
        DbService::init(pgUri, 4);
        std::cout << "[DbService] PostgreSQL client initialized securely." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DbService] Error initializing DbService: " << e.what() << std::endl;
        return 1;
    }

    // Health check endpoints (public, no auth required)
    auto healthHandler = [](const drogon::HttpRequestPtr&,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        Json::Value ret;
        ret["status"] = "healthy";
        ret["service"] = "backend_fast";
        ret["database"] = "connected";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(ret);
        callback(resp);
    };
    drogon::app().registerHandler("/health", healthHandler, {drogon::Get});
    drogon::app().registerHandler("/api/health", healthHandler, {drogon::Get});

    std::cout << "Drogon server listening on port 5000..." << std::endl;
    drogon::app().run();

    return 0;
}
