#include <iostream>
#include <memory>
#include <stdexcept>
#include <drogon/drogon.h>
#include "DatabaseConnection.h" // Assume DatabaseConnection is in its own header
#include "LocationService.h"
#include "PlainRpcDispatcher.h"
#include "RpcController.h"
#include "CorsFilter.h"

using namespace drogon;
using json = nlohmann::json;

// Define DatabaseConnection in its own header or here for simplicity
class DatabaseConnection {
private:
    PGconn* conn_;
public:
    explicit DatabaseConnection(const char* conninfo) {
        conn_ = PQconnectdb(conninfo);
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::string error = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            throw std::runtime_error("Database connection failed: " + error);
        }
    }
    ~DatabaseConnection() { if (conn_) PQfinish(conn_); }
    DatabaseConnection(const DatabaseConnection&) = delete;
    DatabaseConnection& operator=(const DatabaseConnection&) = delete;
    PGconn* get() const { return conn_; }
};


// --- RPC Method Implementations ---

// Helper to convert Location struct to JSON
json locationToJson(const Location& loc) {
    return json{
        {"id", loc.id}, {"name", loc.name}, {"country", loc.country},
        {"state", loc.state}, {"description", loc.description},
        {"svg_link", loc.svg_link}, {"rating", loc.rating}
    };
}

// These functions now depend on a LocationService instance.
json GetTopLocations(LocationService* service, const json& params) {
    int limit = params.value("limit", 10);
    auto locations = service->getTopLocations(limit);
    json arr = json::array();
    for (const auto& loc : locations) arr.push_back(locationToJson(loc));
    return {{"success", true}, {"data", arr}};
}

json GetLocationById(LocationService* service, const json& params) {
    if (!params.contains("id") || !params["id"].is_string())
        throw std::runtime_error("Invalid or missing 'id'");
    auto loc = service->getLocationById(params["id"].get<std::string>());
    return {{"success", true}, {"data", locationToJson(loc)}};
}

json SearchLocations(LocationService* service, const json& params) {
    if (!params.contains("query") || !params["query"].is_string())
        throw std::runtime_error("Invalid or missing 'query'");
    auto results = service->searchLocations(params["query"].get<string>());
    json arr = json::array();
    for (const auto& loc : results) arr.push_back(locationToJson(loc));
    return {{"success", true}, {"data", arr}};
}


int main() {
    try {
        // --- Initialization ---
        const char* conninfo = "postgresql://postgres.vxqsqaysrpxliofqxjyu:the-plus-maps-password"
                               "@aws-0-us-east-2.pooler.supabase.com:5432/postgres?sslmode=require";

        auto db_connection = std::make_unique<DatabaseConnection>(conninfo);
        auto locationService = std::make_unique<LocationService>(db_connection->get());
        
        auto dispatcher = std::make_shared<PlainRpcDispatcher>();

        // Register methods with the dispatcher, passing the locationService instance.
        dispatcher->registerMethod("getTopLocations", 
            [p_service = locationService.get()](const json& p) { return GetTopLocations(p_service, p); });
        dispatcher->registerMethod("getLocationById", 
            [p_service = locationService.get()](const json& p) { return GetLocationById(p_service, p); });
        dispatcher->registerMethod("searchLocations", 
            [p_service = locationService.get()](const json& p) { return SearchLocations(p_service, p); });

        // --- Drogon App Configuration ---
        std::cout << "[Server] Starting on http://0.0.0.0:8080\n";
        
        app()
            // Add a listener for HTTP on port 8080. Drogon will automatically use HTTP/2
            // if a client connects via HTTPS and supports it. For HTTP, it will be HTTP/1.1.
            .addListener("0.0.0.0", 8080)
            
            // Register the CORS filter to apply to all routes ("/*").
            .registerFilter(std::make_shared<CorsFilter>(), "/*", true)

            // Register the RPC controller, passing the dispatcher and DB connection.
            .registerController(std::make_shared<RpcController>(dispatcher, db_connection->get()))
            
            // Optional: Set number of threads. 0 means one per CPU core.
            .setThreadNum(0)
            
            // Run the server.
            .run();

    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << std::endl;
        return 1;
    }
    return 0;
}