#include <drogon/drogon.h>

int main() {
    // Define the /hello endpoint
    drogon::app().registerHandler(
        "/hello",
        [](const drogon::HttpRequestPtr&,
           std::function<void (const drogon::HttpResponsePtr &)> callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
            resp->setBody("hello-world");
            callback(resp);
        },
        {drogon::Get}
    );

    // Bind to port 8080 (Render expects this by default)
    drogon::app().addListener("0.0.0.0", 8080);

    // Run the app
    drogon::app().run();
    return 0;
}
