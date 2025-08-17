#include <drogon/drogon.h>
#include <nlohmann/json.hpp>
#include <postgresql/libpq-fe.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <json/json.h>  // for Json::Value (jsoncpp)

using json = nlohmann::json;

// -------------------- Helper: Convert nlohmann::json → Json::Value --------------------
Json::Value toJsonCpp(const nlohmann::json& j) {
    Json::Value v;
    if (j.isObject()) {
        for (auto& el : j.items()) {
            v[el.key()] = toJsonCpp(el.value());
        }
    } else if (j.isArray()) {
        for (auto& el : j) {
            v.append(toJsonCpp(el));
        }
    } else if (j.isString()) {
        v = j.get<std::string>();
    } else if (j.isBoolean()) {
        v = j.get<bool>();
    } else if (j.isNumberInteger()) {
        v = j.get<long long>();
    } else if (j.isNumberUnsigned()) {
        v = j.get<unsigned long long>();
    } else if (j.isNumberFloat()) {
        v = j.get<double>();
    } else if (j.isNull()) {
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

// -------------------- Location Struct --------------------
struct Location {
    std::string id;
    std::string name;
    std::string country;
    std::string state;
    std::string description;
    std::string svg_link;
    double rating;
};

// -------------------- Location Service --------------------
class LocationService {
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
        return Location{
            PQgetvalue(res, row, 0) ? sanitize(PQgetvalue(res, row, 0)) : "",
            PQgetvalue(res, row, 1) ? sanitize(PQgetvalue(res, row, 1)) : "",
            PQgetvalue(res, row, 2) ? sanitize(PQgetvalue(res, row, 2)) : "",
            PQgetvalue(res, row, 3) ? sanitize(PQgetvalue(res, row, 3)) : "",
            PQgetvalue(res, row, 4) ? sanitize(PQgetvalue(res, row, 4)) : "",
            PQgetvalue(res, row, 5) ? sanitize(PQgetvalue(res, row, 5)) : "",
            PQgetvalue(res, row, 6) ? std::stod(PQgetvalue(res, row, 6)) : 0.0
        };
    }

public:
    LocationService(PGconn* c) : conn_(c) {
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
};

// -------------------- User logging helpers --------------------
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
std::unique_ptr<LocationService> locService;
std::shared_ptr<RpcDispatcher> dispatcher;

// Hardcoded for now
const std::string CONNINFO =
    "postgresql://postgres.vxqsqaysrpxliofqxjyu:the-plus-maps-password"
    "@aws-0-us-east-2.pooler.supabase.com:5432/postgres?sslmode=require";

// -------------------- Convert Location -> JSON --------------------
json locToJson(const Location& l){
    return {
        {"id",l.id},{"name",l.name},{"country",l.country},
        {"state",l.state},{"description",l.description},
        {"svg_link",l.svg_link},{"rating",l.rating}
    };
}

// -------------------- RPC Methods --------------------
json rpcGetTop(const json& p){
    int limit=p.value("limit",10);
    auto v=locService->getTopLocations(limit);
    json arr=json::array(); for(auto& l:v) arr.push_back(locToJson(l));
    return {{"success",true},{"data",arr}};
}
json rpcGetById(const json& p){
    if(!p.contains("id")) throw std::runtime_error("Missing id");
    auto loc=locService->getLocationById(p["id"]);
    return {{"success",true},{"data",locToJson(loc)}};
}
json rpcSearch(const json& p){
    if(!p.contains("query")) throw std::runtime_error("Missing query");
    auto v=locService->searchLocations(p["query"]);
    json arr=json::array(); for(auto& l:v) arr.push_back(locToJson(l));
    return {{"success",true},{"data",arr}};
}

// -------------------- Main --------------------
int main(){
    try{
        dbConn=std::make_unique<DatabaseConnection>(CONNINFO);
        locService=std::make_unique<LocationService>(dbConn->get());

        dispatcher=std::make_shared<RpcDispatcher>();
        dispatcher->reg("getTopLocations",rpcGetTop);
        dispatcher->reg("getLocationById",rpcGetById);
        dispatcher->reg("searchLocations",rpcSearch);

        // Strong CORS middleware
        drogon::app().registerPostHandlingAdvice(
            [](const drogon::HttpRequestPtr&,const drogon::HttpResponsePtr&resp){
                resp->addHeader("Access-Control-Allow-Origin","*");
                resp->addHeader("Access-Control-Allow-Headers","Content-Type,Authorization,X-Requested-With");
                resp->addHeader("Access-Control-Allow-Methods","POST,GET,OPTIONS");
            });

        // Health check
        drogon::app().registerHandler("/health",
            [](const drogon::HttpRequestPtr&,std::function<void(const drogon::HttpResponsePtr&)>&&cb){
                auto r=drogon::HttpResponse::newHttpResponse();
                r->setBody("OK");
                cb(r);
            },{drogon::Get});

        // RPC endpoint
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
