#pragma once

#include <drogon/HttpController.h>
#include <nlohmann/json.hpp>
#include "PlainRpcDispatcher.h"
#include "LocationService.h"
#include "DatabaseConnection.h" // Assuming DatabaseConnection is defined in its own header

using json = nlohmann::json;

class RpcController : public drogon::HttpController<RpcController> {
public:
    // Constructor to receive shared resources.
    RpcController(std::shared_ptr<PlainRpcDispatcher> dispatcher, PGconn* db_conn)
        : dispatcher_(dispatcher), conn_(db_conn) {}

    // Method to handle all RPC calls.
    void handleRpc(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // Define the endpoint mapping.
    // This maps POST requests to /rpc to the handleRpc method.
    METHOD_LIST_BEGIN
    METHOD_ADD(RpcController::handleRpc, "/rpc", drogon::Post);
    METHOD_LIST_END

private:
    // Helper functions for user tracking, moved from the old main.cpp.
    void logUserRequest(const std::string& userid);
    void logUserResponse(const std::string& userid);
    bool isUserBlocked(const std::string& userid);

    std::shared_ptr<PlainRpcDispatcher> dispatcher_;
    PGconn* conn_; // The controller uses the shared database connection.
};

// --- Implementation ---

void RpcController::logUserRequest(const std::string& userid) {
    const char* param[1] = { userid.c_str() };
    PGResultWrapper res(PQexecParams(conn_, "SELECT log_user_request($1);", 1, nullptr, param, nullptr, nullptr, 0));
}

void RpcController::logUserResponse(const std::string& userid) {
    const char* param[1] = { userid.c_str() };
    PGResultWrapper res(PQexecParams(conn_, "SELECT log_user_response($1);", 1, nullptr, param, nullptr, nullptr, 0));
}

bool RpcController::isUserBlocked(const std::string& userid) {
    const char* param[1] = { userid.c_str() };
    PGResultWrapper res(PQexecParams(conn_, "SELECT is_user_blocked($1);", 1, nullptr, param, nullptr, nullptr, 0));
    if (PQresultStatus(res.get()) != PGRES_TUPLES_OK || PQntuples(res.get()) == 0) {
        return false;
    }
    bool blocked = (strcmp(PQgetvalue(res.get(), 0, 0), "t") == 0);
    return blocked;
}

void RpcController::handleRpc(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    json request_body;
    try {
        // Drogon uses jsoncpp, but our logic uses nlohmann::json.
        // We parse the raw request body string with nlohmann::json to maintain compatibility.
        request_body = json::parse(req->body());
    } catch (const std::exception& e) {
        json err_resp = {{"success", false}, {"error", "Invalid JSON"}};
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err_resp.dump());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string userid;
    if (request_body.value("params", json::object()).contains("userid")) {
        userid = request_body["params"]["userid"].get<std::string>();

        if (!userid.empty()) {
            // Check if user is blocked before proceeding.
            if (isUserBlocked(userid)) {
                json err_resp = {{"success", false}, {"error", "You have exceeded the rate limit"}};
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err_resp.dump());
                resp->setStatusCode(drogon::k429TooManyRequests);
                callback(resp);
                return;
            }
            logUserRequest(userid);
        }
    }

    json response_body;
    try {
        // Dispatch the request to the appropriate handler.
        response_body = dispatcher_->dispatch(request_body);
        if (!userid.empty()) {
            logUserResponse(userid);
        }
    } catch (const std::exception& e) {
        response_body = {{"success", false}, {"error", e.what()}};
    }
    
    // Create and send the JSON response.
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response_body.dump());
    callback(resp);
}