// main.cpp
#include "src/headers/FlaskCpp.h"
#include <cstdlib> // Для atoi
#include <csignal> // Для обработки сигналов
#include <thread>
#include <atomic>

std::atomic<bool> globalRunning(true);

// Обработчик сигнала
void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    globalRunning = false;
}

int main(int argc, char* argv[]) {
    // Установка обработчика сигналов
    // Обратите внимание, что SIGINT и SIGTERM более подходят для graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    // SIGKILL не может быть перехвачен, поэтому его использование здесь не имеет смысла
    // std::signal(SIGKILL, signalHandler); // Неправильно, пропустите

    int port = 8080;
    bool verbose = false;
    bool enableHotReload = true; // По умолчанию hot_reload включен
    size_t minThreads = 2;
    size_t maxThreads = 8;

    // Простейшая обработка аргументов командной строки
    for(int i = 1; i < argc; ++i){
        std::string arg = argv[i];
        if(arg == "--port" && i + 1 < argc){
            port = std::atoi(argv[++i]);
        }
        else if(arg == "--verbose"){
            verbose = true;
        }
        else if(arg == "--no-hot-reload"){
            enableHotReload = false;
        }
        else if(arg == "--threads-min" && i + 1 < argc){
            minThreads = std::atoi(argv[++i]);
        }
        else if(arg == "--threads-max" && i + 1 < argc){
            maxThreads = std::atoi(argv[++i]);
        }
    }

    // Проверка корректности значений
    if(minThreads > maxThreads){
        std::cerr << "Минимальное количество потоков не может быть больше максимального." << std::endl;
        return 1;
    }

    FlaskCpp app(port, verbose, enableHotReload, minThreads, maxThreads);

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

    // Страница с наследованием "/extend"
    app.routeParam("/extend", [&](const RequestData& req) -> std::string {
        TemplateEngine::Context ctx {
            {"title", std::string("Страница с Наследованием")},
            {"show", true},
            {"message", std::string("Это страница, которая наследует базовый шаблон.")}
        };
        std::string body = app.renderTemplate("extend.html", ctx);
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

        // Формируем заголовок Set-Cookie
        std::vector<std::pair<std::string, std::string>> extra_headers;
        extra_headers.emplace_back("Set-Cookie", app.setCookie("User", "JohnDoe", "/", "", true, false, "Lax"));

        // Если нужно установить несколько куки, добавляем их сюда
        // Пример:
        extra_headers.emplace_back("Set-Cookie", app.setCookie("SessionID", "abc123", "/", "", true, true, "Strict"));

        // Формируем ответ с дополнительными заголовками
        return app.buildResponse("200 OK", "text/html", body, extra_headers);
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

        // Формируем заголовок для удаления куки
        std::vector<std::pair<std::string, std::string>> extra_headers;
        extra_headers.emplace_back("Set-Cookie", app.deleteCookie("User", "/"));

        // Формируем ответ с дополнительными заголовками
        return app.buildResponse("200 OK", "text/html", body, extra_headers);
    });

    // Запуск сервера асинхронно
    app.runAsync();

    // Ожидание сигнала для остановки сервера
    while(globalRunning){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Остановка сервера
    app.stop();

    std::cout << "Server stopped gracefully." << std::endl;
    return 0;
}
