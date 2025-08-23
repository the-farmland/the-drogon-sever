// File: main.cpp

#include <drogon/drogon.h>
#include <nlohmann/json.hpp>
#include <postgresql/libpq-fe.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <json/json.h> // for Json::Value (jsoncpp)

using json = nlohmann::json;

// -------------------- Helper: Convert nlohmann::json → Json::Value --------------------
Json::Value toJsonCpp(const nlohmann::json& j) {
    Json::Value v;
    if (j.is_object()) {
        for (auto& el : j.items()) {
            v[el.key()] = toJsonCpp(el.value());
        }
    } else if (j.is_array()) {
        for (auto& el : j) {
            v.append(toJsonCpp(el));
        }
    } else if (j.is_string()) {
        v = j.get<std::string>();
    } else if (j.is_boolean()) {
        v = j.get<bool>();
    } else if (j.is_number_integer()) {
        v = static_cast<Json::Int64>(j.get<long long>());
    } else if (j.is_number_unsigned()) {
        v = static_cast<Json::UInt64>(j.get<unsigned long long>());
    } else if (j.is_number_float()) {
        v = j.get<double>();
    } else if (j.is_null()) {
        v = Json::nullValue;
    }
    return v;
}

// -------------------- Database Wrapper --------------------
class DatabaseConnection {
private:
    PGconn* conn_;
public:
    explicit DatabaseConnection(const std::string &conninfo) {
        conn_ = PQconnectdb(conninfo.c_str());
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::string err = PQerrorMessage(conn_);
            PQfinish(conn_);
            throw std::runtime_error("Database connection failed: " + err);
        }
    }
    ~DatabaseConnection() {
        if (conn_) PQfinish(conn_);
    }
    PGconn* get() const { return conn_; }
};

// -------------------- Data Structs --------------------
struct Location {
    std::string id;
    std::string name;
    std::string country;
    std::string state;
    std::string description;
    std::string svg_link;
    double rating;
    // Assuming these columns exist as per your original main.cpp
    std::string map_main_image;
    std::string map_cover_image;
    std::string main_background_image;
    std::string map_full_address;
    std::string map_png_link;
    // NEW FIELD for scoreboard/leaderboard data
    std::string boards; // Stored as a JSON string
};

struct Chart {
    int id;
    std::string location_id;
    std::string chart_type;
    std::string title;
    std::string chart_data; // Stored as a JSON string
};


// -------------------- Location Service --------------------
class AppService {
private:
    PGconn* conn_;
    mutable std::mutex db_mutex_;

    std::string sanitize(const std::string& s) const {
        std::string out = s;
        out.erase(remove_if(out.begin(), out.end(),
                            [](unsigned char c){ return c < 32 && c != '\t' && c != '\n' && c != '\r'; }),
                  out.end());
        return out;
    }

    Location rowToLocation(PGresult* res, int row) const {
        // Column order must match the table definition, now with 13 columns
        return Location{
            PQgetvalue(res, row, 0) ? sanitize(PQgetvalue(res, row, 0)) : "", // id
            PQgetvalue(res, row, 1) ? sanitize(PQgetvalue(res, row, 1)) : "", // name
            PQgetvalue(res, row, 2) ? sanitize(PQgetvalue(res, row, 2)) : "", // country
            PQgetvalue(res, row, 3) ? sanitize(PQgetvalue(res, row, 3)) : "", // state
            PQgetvalue(res, row, 4) ? sanitize(PQgetvalue(res, row, 4)) : "", // description
            PQgetvalue(res, row, 5) ? sanitize(PQgetvalue(res, row, 5)) : "", // svg_link
            PQgetvalue(res, row, 6) ? std::stod(PQgetvalue(res, row, 6)) : 0.0, // rating
            // Assuming columns 7-11 exist as per original main.cpp
            PQgetvalue(res, row, 7) ? sanitize(PQgetvalue(res, row, 7)) : "", 
            PQgetvalue(res, row, 8) ? sanitize(PQgetvalue(res, row, 8)) : "", 
            PQgetvalue(res, row, 9) ? sanitize(PQgetvalue(res, row, 9)) : "", 
            PQgetvalue(res, row,10) ? sanitize(PQgetvalue(res, row,10)) : "", 
            PQgetvalue(res, row,11) ? sanitize(PQgetvalue(res, row,11)) : "",
            // New 'boards' column at index 12
            PQgetvalue(res, row, 12) ? sanitize(PQgetvalue(res, row, 12)) : "" // boards
        };
    }

    Chart rowToChart(PGresult* res, int row) const {
        return Chart{
            PQgetvalue(res, row, 0) ? std::stoi(PQgetvalue(res, row, 0)) : 0,      // id
            PQgetvalue(res, row, 1) ? sanitize(PQgetvalue(res, row, 1)) : "",     // location_id
            PQgetvalue(res, row, 2) ? sanitize(PQgetvalue(res, row, 2)) : "",     // chart_type
            PQgetvalue(res, row, 3) ? sanitize(PQgetvalue(res, row, 3)) : "",     // title
            PQgetvalue(res, row, 4) ? sanitize(PQgetvalue(res, row, 4)) : ""      // chart_data
        };
    }


public:
    AppService(PGconn* c) : conn_(c) {
        if (!c) throw std::runtime_error("Invalid DB connection");
    }

    std::vector<Location> getTopLocations(int limit) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        std::string sql = "SELECT * FROM get_top_locations($1);";
        std::string lim = std::to_string(limit);
        const char* vals[1] = {lim.c_str()};

        PGresult* res = PQexecParams(conn_, sql.c_str(), 1, nullptr, vals, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            throw std::runtime_error("Query failed: " + err);
        }
        std::vector<Location> list;
        int rows = PQntuples(res);
        for (int i=0;i<rows;i++) list.push_back(rowToLocation(res,i));
        PQclear(res);
        return list;
    }

    Location getLocationById(const std::string& id) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        const char* vals[1] = {id.c_str()};
        PGresult* res = PQexecParams(conn_, "SELECT * FROM get_location_by_id($1);",1,nullptr,vals,nullptr,nullptr,0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res)==0) {
            PQclear(res);
            throw std::runtime_error("Location not found");
        }
        Location loc = rowToLocation(res,0);
        PQclear(res);
        return loc;
    }

    std::vector<Location> searchLocations(const std::string& q) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        const char* vals[1] = {q.c_str()};
        PGresult* res = PQexecParams(conn_, "SELECT * FROM search_locations($1);",1,nullptr,vals,nullptr,nullptr,0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            throw std::runtime_error("Search failed: " + err);
        }
        std::vector<Location> list;
        int rows = PQntuples(res);
        for (int i=0;i<rows;i++) list.push_back(rowToLocation(res,i));
        PQclear(res);
        return list;
    }

    std::vector<Chart> getChartsForLocation(const std::string& locationId) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        const char* vals[1] = {locationId.c_str()};
        PGresult* res = PQexecParams(conn_, "SELECT id, location_id, chart_type, title, chart_data FROM charts WHERE location_id = $1 ORDER BY id;", 1, nullptr, vals, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            throw std::runtime_error("Chart query failed: " + err);
        }
        std::vector<Chart> list;
        int rows = PQntuples(res);
        for (int i=0; i<rows; ++i) list.push_back(rowToChart(res, i));
        PQclear(res);
        return list;
    }
};

// -------------------- User logging helpers (unchanged) --------------------
void logUserRequest(PGconn* conn,const std::string& uid){
    const char* vals[1]={uid.c_str()};
    PGresult* res=PQexecParams(conn,"SELECT log_user_request($1);",1,nullptr,vals,nullptr,nullptr,0);
    PQclear(res);
}
void logUserResponse(PGconn* conn,const std::string& uid){
    const char* vals[1]={uid.c_str()};
    PGresult* res=PQexecParams(conn,"SELECT log_user_response($1);",1,nullptr,vals,nullptr,nullptr,0);
    PQclear(res);
}
bool isUserBlocked(PGconn* conn,const std::string& uid){
    const char* vals[1]={uid.c_str()};
    PGresult* res=PQexecParams(conn,"SELECT is_user_blocked($1);",1,nullptr,vals,nullptr,nullptr,0);
    if (PQresultStatus(res)!=PGRES_TUPLES_OK||PQntuples(res)==0){PQclear(res);return false;}
    bool blocked=(strcmp(PQgetvalue(res,0,0),"t")==0);
    PQclear(res);
    return blocked;
}

// -------------------- JSON-RPC Dispatcher --------------------
class RpcDispatcher {
    using Handler = std::function<json(const json&)>;
    std::unordered_map<std::string,Handler> methods_;
public:
    void reg(const std::string& name, Handler h){
        methods_[name]=h;
    }
    json dispatch(const json& req){
        if (!req.contains("method")||!req["method"].is_string())
            throw std::runtime_error("Missing method");
        if (!req.contains("params")||!req["params"].is_object())
            throw std::runtime_error("Missing params");
        auto it=methods_.find(req["method"]);
        if(it==methods_.end()) throw std::runtime_error("Method not found");
        return it->second(req["params"]);
    }
};

// -------------------- Globals --------------------
std::unique_ptr<DatabaseConnection> dbConn;
std::unique_ptr<AppService> appService; // Renamed for clarity
std::shared_ptr<RpcDispatcher> dispatcher;

const std::string CONNINFO =
    "postgresql://postgres.vxqsqaysrpxliofqxjyu:the-plus-maps-password"
    "@aws-0-us-east-2.pooler.supabase.com:5432/postgres?sslmode=require";

// -------------------- Convert Struct -> JSON --------------------
json locToJson(const Location& l){
    json j = {
        {"id",l.id},{"name",l.name},{"country",l.country},
        {"state",l.state},{"description",l.description},
        {"svg_link",l.svg_link},{"rating",l.rating},
        {"map_main_image", l.map_main_image},
        {"map_cover_image", l.map_cover_image},
        {"main_background_image", l.main_background_image},
        {"map_full_address", l.map_full_address},
        {"map_png_link", l.map_png_link}
    };
    // Parse the boards JSON string into a JSON object if it's not empty
    if (!l.boards.empty()) {
        try {
            j["boards"] = json::parse(l.boards);
        } catch (const std::exception& e) {
            j["boards"] = nullptr; // Or some error indicator
        }
    } else {
        j["boards"] = nullptr;
    }
    return j;
}

json chartToJson(const Chart& c) {
    json j = {
        {"id", c.id}, {"location_id", c.location_id},
        {"chart_type", c.chart_type}, {"title", c.title}
    };
    // Parse the chart_data JSON string
    if (!c.chart_data.empty()) {
        try {
            j["chart_data"] = json::parse(c.chart_data);
        } catch (const std::exception& e) {
            j["chart_data"] = nullptr;
        }
    } else {
        j["chart_data"] = nullptr;
    }
    return j;
}


// -------------------- RPC Methods --------------------
json rpcGetTop(const json& p){
    int limit=p.value("limit",10);
    auto v=appService->getTopLocations(limit);
    json arr=json::array(); for(auto& l:v) arr.push_back(locToJson(l));
    return {{"success",true},{"data",arr}};
}
json rpcGetById(const json& p){
    if(!p.contains("id")) throw std::runtime_error("Missing id");
    auto loc=appService->getLocationById(p["id"]);
    return {{"success",true},{"data",locToJson(loc)}};
}
json rpcSearch(const json& p){
    if(!p.contains("query")) throw std::runtime_error("Missing query");
    auto v=appService->searchLocations(p["query"]);
    json arr=json::array(); for(auto& l:v) arr.push_back(locToJson(l));
    return {{"success",true},{"data",arr}};
}
json rpcGetCharts(const json& p) {
    if (!p.contains("locationId")) throw std::runtime_error("Missing locationId");
    auto v = appService->getChartsForLocation(p["locationId"]);
    json arr = json::array();
    for(auto& c : v) arr.push_back(chartToJson(c));
    return {{"success", true}, {"data", arr}};
}


// -------------------- Main --------------------
int main(){
    try{
        dbConn=std::make_unique<DatabaseConnection>(CONNINFO);
        appService=std::make_unique<AppService>(dbConn->get());

        dispatcher=std::make_shared<RpcDispatcher>();
        dispatcher->reg("getTopLocations",rpcGetTop);
        dispatcher->reg("getLocationById",rpcGetById);
        dispatcher->reg("searchLocations",rpcSearch);
        dispatcher->reg("getChartsForLocation", rpcGetCharts); // Register new method

        drogon::app().registerPostHandlingAdvice(
            [](const drogon::HttpRequestPtr&,const drogon::HttpResponsePtr&resp){
                resp->addHeader("Access-Control-Allow-Origin","*");
                resp->addHeader("Access-Control-Allow-Headers","Content-Type,Authorization,X-Requested-With");
                resp->addHeader("Access-Control-Allow-Methods","POST,GET,OPTIONS");
            });

        drogon::app().registerHandler("/health",
            [](const drogon::HttpRequestPtr&,std::function<void(const drogon::HttpResponsePtr&)>&&cb){
                auto r=drogon::HttpResponse::newHttpResponse();
                r->setBody("OK");
                cb(r);
            },{drogon::Get});

        drogon::app().registerHandler("/rpc",
            [](const drogon::HttpRequestPtr &req,std::function<void(const drogon::HttpResponsePtr&)> &&cb){
                try {
                    auto body = req->getBody();
                    auto j = json::parse(body);

                    std::string uid;
                    if(j["params"].contains("userid")) uid = j["params"]["userid"];

                    if(!uid.empty()){
                        if(isUserBlocked(dbConn->get(), uid)){
                            json err = {{"success",false},{"error","Rate limit exceeded"}};
                            auto resp = drogon::HttpResponse::newHttpJsonResponse(toJsonCpp(err));
                            resp->setStatusCode(drogon::k429TooManyRequests);
                            cb(resp);
                            return;
                        }
                        logUserRequest(dbConn->get(), uid);
                    }

                    auto out = dispatcher->dispatch(j);
                    if(!uid.empty()) logUserResponse(dbConn->get(), uid);

                    auto resp = drogon::HttpResponse::newHttpJsonResponse(toJsonCpp(out));
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    cb(resp);

                } catch(const std::exception &e){
                    json err = {{"success",false},{"error",e.what()}};
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(toJsonCpp(err));
                    resp->setStatusCode(drogon::k400BadRequest);
                    cb(resp);
                }
            },{drogon::Post,drogon::Options});

        std::cout<<"Server running on 0.0.0.0:8080\n";
        drogon::app().addListener("0.0.0.0",8080).run();

    }catch(std::exception&e){
        std::cerr<<"Fatal: "<<e.what()<<std::endl;
        return 1;
    }
}
