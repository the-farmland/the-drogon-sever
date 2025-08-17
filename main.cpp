#include <drogon/drogon.h>
#include <cstdlib>   // for std::getenv
#include <string>

int main() {
    // Define the /hello endpoint
    drogon::app().registerHandler(
        "/hello",
        [](drogon::HttpRequestPtr req,
           std::function<void(drogon::HttpResponsePtr)> callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
            resp->setBody("hello-world");
            callback(resp);
        },
        {drogon::Get}
    );

    // Bind to $PORT (Render sets this dynamically), fallback 8080
    const char* portStr = std::getenv("PORT");
    uint16_t port = portStr ? static_cast<uint16_t>(std::stoi(portStr)) : 8080;
    drogon::app().addListener("0.0.0.0", port);

    drogon::app().run();
    return 0;
}
