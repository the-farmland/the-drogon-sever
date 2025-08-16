#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <stdexcept>
#include <postgresql/libpq-fe.h>
#include <nlohmann/json.hpp>
#include <drogon/drogon.h>
#include "PlainRpcDispatcher.h"
#include "LocationService.h"
#include <cstdlib>
#include <set>

using json = nlohmann::json;
using namespace std;

class DatabaseConnection {
private:
    PGconn* conn_;
public:
    explicit DatabaseConnection(const char* conninfo) {
        conn_ = PQconnectdb(conninfo);
        if (PQstatus(conn_) != CONNECTION_OK) {
            string error = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            throw runtime_error("Database connection failed: " + error);
        }
    }
    ~DatabaseConnection() { if (conn_) PQfinish(conn_); }
    DatabaseConnection(const DatabaseConnection&) = delete;
    DatabaseConnection& operator=(const DatabaseConnection&) = delete;
    PGconn* get() const { return conn_; }
    bool isValid() const { return conn_ && PQstatus(conn_) == CONNECTION_OK; }
};

shared_ptr<DatabaseConnection> db_connection;
shared_ptr<LocationService> locationService;
string global_conninfo;

bool ensureDbConnection(int retries = 5, int delayMs = 2000) {
    for (int i = 0; i < retries; i++) {
        if (!db_connection || !db_connection->isValid()) {
            try {
                db_connection = make_shared<DatabaseConnection>(global_conninfo.c_str());
                locationService = make_shared<LocationService>(db_connection->get());
                cout << "[DB] Connected to database.\n";
                return true;
            } catch (const exception& e) {
                cerr << "[DB] Connection attempt " << i+1 << " failed: " << e.what() << endl;
                this_thread::sleep_for(chrono::milliseconds(delayMs));
            }
        } else {
            return true;
        }
    }
    return false;
}

json locationToJson(const Location& loc) {
    return json{
        {"id", loc.id},
        {"name", loc.name},
        {"country", loc.country},
        {"state", loc.state},
        {"description", loc.description},
        {"svg_link", loc.svg_link},
        {"rating", loc.rating}
    };
}

void logUserRequest(PGconn* conn, const string& userid) {
    const char* param[1] = { userid.c_str() };
    PGresult* res = PQexecParams(conn, "SELECT log_user_request($1);", 1, nullptr, param, nullptr, nullptr, 0);
    PQclear(res);
}

void logUserResponse(PGconn* conn, const string& userid) {
    const char* param[1] = { userid.c_str() };
    PGresult* res = PQexecParams(conn, "SELECT log_user_response($1);", 1, nullptr, param, nullptr, nullptr, 0);
    PQclear(res);
}

bool isUserBlocked(PGconn* conn, const string& userid) {
    const char* param[1] = { userid.c_str() };
    PGresult* res = PQexecParams(conn, "SELECT is_user_blocked($1);", 1, nullptr, param, nullptr, nullptr, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return false;
    }
    bool blocked = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
    PQclear(res);
    return blocked;
}

json GetTopLocations(const json& params) {
    int limit = params.value("limit", 10);
    auto locations = locationService->getTopLocations(limit);
    json arr = json::array();
    for (const auto& loc : locations) arr.push_back(locationToJson(loc));
    return {{"success", true}, {"data", arr}};
}

json GetLocationById(const json& params) {
    if (!params.contains("id") || !params["id"].is_string())
        throw runtime_error("Invalid or missing 'id'");
    auto loc = locationService->getLocationById(params["id"].get<string>());
    return {{"success", true}, {"data", locationToJson(loc)}};
}

json SearchLocations(const json& params) {
    if (!params.contains("query") || !params["query"].is_string())
        throw runtime_error("Invalid or missing 'query'");
    auto results = locationService->searchLocations(params["query"].get<string>());
    json arr = json::array();
    for (const auto& loc : results) arr.push_back(locationToJson(loc));
    return {{"success", true}, {"data", arr}};
}

int main() {
    global_conninfo =
        "postgresql://postgres.vxqsqaysrpxliofqxjyu:the-plus-maps-password"
        "@aws-0-us-east-2.pooler.supabase.com:5432/postgres?sslmode=require";

    ensureDbConnection();

    auto dispatcher = make_shared<PlainRpcDispatcher>();
    dispatcher->registerMethod("getTopLocations", GetTopLocations);
    dispatcher->registerMethod("getLocationById", GetLocationById);
    dispatcher->registerMethod("searchLocations", SearchLocations);

    // Set up CORS
    drogon::app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp) {
        std::string origin = req->getHeader("Origin");
        std::set<std::string> allowed_origins;
        const char* env_origins = std::getenv("ALLOWED_ORIGINS");
        std::string origins_str = env_origins ? env_origins : "https://the-super-sweet-two.vercel.app,http://localhost:3000";
        size_t pos = 0;
        while ((pos = origins_str.find(',')) != std::string::npos) {
            allowed_origins.insert(origins_str.substr(0, pos));
            origins_str.erase(0, pos + 1);
        }
        allowed_origins.insert(origins_str); // last one
        
        if (!origin.empty() && allowed_origins.count(origin)) {
            resp->addHeader("Access-Control-Allow-Origin", origin);
            resp->addHeader("Vary", "Origin");
        }
        resp->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        resp->addHeader("Access-Control-Max-Age", "86400");
    });

    // Health check endpoint
    drogon::app().registerHandler("/health", [](const drogon::HttpRequestPtr&,
                                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setBody("OK");
        callback(resp);
    });

    // Main RPC endpoint
    drogon::app().registerHandler("/rpc",
        [dispatcher](const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);

            if (!ensureDbConnection()) {
                resp->setStatusCode(drogon::HttpStatusCode::k503ServiceUnavailable);
                resp->setBody(json{{"success", false}, {"error", "Database unavailable"}}.dump());
                callback(resp);
                return;
            }

            json request;
            try {
                request = json::parse(req->getBody());
            } catch (...) {
                resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
                resp->setBody(json{{"success", false}, {"error", "Invalid JSON"}}.dump());
                callback(resp);
                return;
            }

            string userid;
            if (request["params"].contains("userid") && request["params"]["userid"].is_string()) {
                userid = request["params"]["userid"].get<string>();

                if (!userid.empty()) {
                    if (isUserBlocked(db_connection->get(), userid)) {
                        resp->setStatusCode(drogon::HttpStatusCode::k429TooManyRequests);
                        resp->setBody(json{{"success", false}, {"error", "You have exceeded the rate limit"}}.dump());
                        callback(resp);
                        return;
                    }
                    logUserRequest(db_connection->get(), userid);
                    if (isUserBlocked(db_connection->get(), userid)) {
                        resp->setStatusCode(drogon::HttpStatusCode::k429TooManyRequests);
                        resp->setBody(json{{"success", false}, {"error", "You have exceeded the rate limit"}}.dump());
                        callback(resp);
                        return;
                    }
                }
            }

            json response = dispatcher->dispatch(request);
            if (!userid.empty()) logUserResponse(db_connection->get(), userid);
            resp->setBody(response.dump());
            callback(resp);
        },
        {drogon::Post, drogon::Options});

    // Start server
    drogon::app().addListener("0.0.0.0", 8080);
    LOG_INFO << "Server running on http://0.0.0.0:8080";
    drogon::app().run();

    return 0;
}