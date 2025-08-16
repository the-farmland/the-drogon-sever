#pragma once
#include <drogon/HttpFilter.h>
#include <set>
#include <string>

class CorsFilter : public drogon::HttpFilter<CorsFilter> {
public:
    CorsFilter() {}

    virtual void doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) override {
        // Hardcoded list of allowed origins.
        const std::set<std::string> allowed_origins = {
            "https://the-super-sweet-two.vercel.app",
            "http://localhost:3000",
            "http://127.0.0.1:3000"
        };

        // Create a response pointer to add headers to.
        // For preflight OPTIONS requests, we create a new response.
        // For other requests, we'll get the response from the next handler in the chain.
        drogon::HttpResponsePtr response;

        std::string origin = req->getHeader("Origin");
        std::string allowed_origin_header;

        // If the request's origin is in our allowed list, set the response header.
        if (!origin.empty() && allowed_origins.count(origin)) {
            allowed_origin_header = origin;
        }

        // Handle preflight (OPTIONS) requests.
        if (req->method() == drogon::HttpMethod::Options) {
            response = drogon::HttpResponse::newHttpResponse();
            response->setStatusCode(drogon::k204NoContent);
        }

        // Add headers to the response if it exists (i.e., for OPTIONS requests).
        if (response) {
            if (!allowed_origin_header.empty()) {
                response->addHeader("Access-Control-Allow-Origin", allowed_origin_header);
                response->addHeader("Vary", "Origin");
            }
            response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
            response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            response->addHeader("Access-Control-Max-Age", "86400"); // 24 hours
            fcb(response);
            return;
        }

        // For non-OPTIONS requests, pass control to the next handler in the chain.
        fccb();

        // After the handler has generated a response, add CORS headers to it.
        response = drogon::app().getThreadLocalResponse();
        if (response && !allowed_origin_header.empty()) {
            response->addHeader("Access-Control-Allow-Origin", allowed_origin_header);
            response->addHeader("Vary", "Origin");
        }
    }
};