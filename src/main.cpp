#include "headers/FlaskCpp.h"
#include "headers/Utils.h"

int main() {
    FlaskCpp app(8080, true);

    // Загрузка шаблонов из директории "templates"
    app.loadTemplatesFromDirectory("templates");

    // Добавление маршрутов
    app.route("/", [&](const RequestData& req) -> std::string {
        TemplateEngine::Context ctx {
            {"title", std::string("Добро пожаловать")},
            {"show", true},
            {"message", std::string("<b>Привет, мир!</b>")},
            {"items", std::vector<std::map<std::string,std::string>>{
                {{"field","Элемент 1"}},
                {{"field","Элемент 2"}},
                {{"field","Элемент 3"}}
            }},
            {"note", std::string("Это примечание из частичного шаблона.")}
        };

        std::string body = app.renderTemplate("main.html", ctx);

        return app.buildResponse("200 OK", "text/html", body);
    });

    app.route("/form", [&](const RequestData& req) -> std::string {
        std::string body = app.renderTemplate("form.html", {});
        return app.buildResponse("200 OK", "text/html", body);
    });

    app.route("/submit", [&](const RequestData& req) -> std::string {
        std::string user = "";
        auto it = req.formData.find("username");
        if (it != req.formData.end()) user = it->second;

        // Формирование ответа с использованием шаблона
        TemplateEngine::Context ctx {
            {"title", std::string("Приветствие")},
            {"message", "Привет, " + user + "!"}
        };
        std::string body = "<h1>" + std::get<std::string>(ctx["message"]) + "</h1><a href=\"/\">Назад</a>";
        return app.buildResponse("200 OK", "text/html", body);
    });

    app.routeParam("/user/<id>", [&](const RequestData& req) -> std::string {
        TemplateEngine::Context ctx {
            {"userId", req.routeParams.at("id")}
        };
        std::string body = app.renderTemplate("user.html", ctx);
        return app.buildResponse("200 OK", "text/html", body);
    });

    app.route("/api/data", [&](const RequestData& req) -> std::string {
        std::string json = R"({"status":"ok","message":"Hello from JSON!"})";
        return app.buildResponse("200 OK", "application/json", json);
    });

    app.route("/error", [&](const RequestData& req) -> std::string {
        throw std::runtime_error("Тестовая ошибка");
        return std::string();
    });

    // Маршруты для работы с Cookies

    // Установка cookie
    app.route("/set_cookie", [&](const RequestData& req) -> std::string {
        std::string body = "<h1>Cookie Set</h1><p>Cookie 'User' был установлен.</p>";
        std::ostringstream responseStream;
        app.setCookie(responseStream, "User", "JohnDoe", "/", "", true, false, "Lax");
        std::string responseHeaders = responseStream.str();
        return responseHeaders + app.buildResponse("200 OK", "text/html", body);
    });


    // Получение cookie
    app.route("/get_cookie", [&](const RequestData& req) -> std::string {
        std::string body = "<h1>Get Cookie</h1>";
        auto it = req.cookies.find("User");
        if (it != req.cookies.end()) {
            body += "<p>Cookie 'User' = " + it->second + "</p>";
        } else {
            body += "<p>Cookie 'User' не найден.</p>";
        }

        return app.buildResponse("200 OK", "text/html", body);
    });

    // Удаление cookie
    app.route("/delete_cookie", [&](const RequestData& req) -> std::string {
        std::string body = "<h1>Cookie Deleted</h1><p>Cookie 'User' был удалён.</p>";
        std::ostringstream responseStream;
        app.deleteCookie(responseStream, "User", "/");
        std::string responseHeaders = responseStream.str();
        return responseHeaders + app.buildResponse("200 OK", "text/html", body);
    });

    app.run();
    return 0;
}
