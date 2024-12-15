// headers/FlaskCpp.h
#ifndef FLASKCPP_H
#define FLASKCPP_H

#include <iostream>
#include <unordered_map>
#include <functional>
#include <string>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <atomic> 
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <cstdlib>      // Для system
#include <cstdio>       // Для popen, pclose
#include <memory>
#include <array>

#include "TemplateEngine.h"
#include "ThreadPool.h" // Добавляем пул потоков

// Структура для хранения данных запроса
struct RequestData {
    std::string method;
    std::string path;
    std::map<std::string, std::string> queryParams;
    std::map<std::string, std::string> formData; // Для POST-запросов
    std::map<std::string, std::string> routeParams; // Параметры из пути: /user/<id>
    std::map<std::string, std::string> headers;
    std::string body;
    std::map<std::string, std::string> cookies; // Хранит парсенные cookies
};

// Типы хендлеров маршрутов
using SimpleHandler = std::function<std::string(const RequestData&)>;
using ComplexHandler = std::function<std::string(const RequestData&)>;

class FlaskCpp {
public:
    // Обновленный конструктор с дополнительными параметрами для пула потоков
    FlaskCpp(int port, bool verbose = false, bool enableHotReload = true, size_t minThreads = 2, size_t maxThreads = 8);

    void setTemplate(const std::string& name, const std::string& content);

    // Добавление маршрута без параметров
    void route(const std::string& path, SimpleHandler handler);

    // Добавление маршрута с параметрами, например: /user/<id>
    void routeParam(const std::string& pattern, ComplexHandler handler);

    // Загрузка шаблонов из директории
    void loadTemplatesFromDirectory(const std::string& directoryPath);

    // Метод для запуска сервера в отдельном потоке
    void runAsync();

    // Метод для запуска сервера синхронно (блокирующий)
    void run();

    void stop(); // Новый метод для остановки сервера

    std::string renderTemplate(const std::string& templateName, const TemplateEngine::Context& context);

    // Вспомогательная функция для формирования HTTP-ответов
    // Теперь принимает вектор пар заголовков для поддержки нескольких заголовков с одинаковым именем
    std::string buildResponse(const std::string& status_code,
                              const std::string& content_type,
                              const std::string& body,
                              const std::vector<std::pair<std::string, std::string>>& extra_headers = {});

    // Функции для управления cookies

    // Установка cookie: возвращает строку заголовка Set-Cookie
    std::string setCookie(const std::string& name, const std::string& value,
                          const std::string& path = "/", const std::string& expires = "",
                          bool httpOnly = true, bool secure = false, const std::string& sameSite = "Lax");

    // Удаление cookie: возвращает строку заголовка Set-Cookie для удаления куки
    std::string deleteCookie(const std::string& name,
                             const std::string& path = "/");

#ifdef ENABLE_PHP
    // Дополнительная функция для поддержки PHP через php-cgi
    std::string executePHP(const RequestData& reqData, const std::filesystem::path& scriptPath);
#endif

private:
    int port;
    bool verbose;
    bool enableHotReload; // Новый флаг для управления hot_reload
    TemplateEngine templateEngine; 
    std::string templatesDirectory;
    std::map<std::string, std::filesystem::file_time_type> templatesTimestamps;
    std::atomic<bool> running; // Для остановки потока

    // Пул потоков
    ThreadPool threadPool;

    // Поток для мониторинга шаблонов (hot reload)
    std::thread hotReloadThread;

    void monitorTemplates();

    struct ParamRoute {
        std::string pattern;
        ComplexHandler handler;
    };

    std::unordered_map<std::string, ComplexHandler> routes;
    std::vector<ParamRoute> paramRoutes;
    std::mutex routeMutex;

    void handleClient(int clientSocket, const std::string& clientIP);
    std::string readRequest(int clientSocket);
    void parseRequest(const std::string& request, RequestData& reqData);
    void parseQueryString(const std::string& queryString, std::map<std::string, std::string>& queryParams);
    bool matchParamRoute(const std::string& path, const std::string& pattern, std::map<std::string,std::string>& routeParams);
    bool serveStaticFile(const RequestData& reqData, std::string& response);
    void sendResponse(int clientSocket, const std::string& content);
    std::string generate404Error();
    std::string generate500Error(const std::string& msg);

    void parseCookies(const std::string& cookieHeader, std::map<std::string, std::string>& cookies);
    std::string urlDecode(const std::string &value);
};

#endif // FLASKCPP_H
