#ifndef FLASKCPP_H
#define FLASKCPP_H

#include <iostream>
#include <unordered_map>
#include <functional>
#include <string>
#include <algorithm>
#include <sstream>
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

// Включение TemplateEngine
#include "TemplateEngine.h"

// Включение Utils
#include "Utils.h"

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
    FlaskCpp(int port, bool verbose = false);

    void setTemplate(const std::string& name, const std::string& content);

    // Добавление маршрута без параметров
    void route(const std::string& path, SimpleHandler handler);

    // Добавление маршрута с параметрами, например: /user/<id>
    void routeParam(const std::string& pattern, ComplexHandler handler);

    // Загрузка шаблонов из директории
    void loadTemplatesFromDirectory(const std::string& directoryPath);

    void run();

    std::string renderTemplate(const std::string& templateName, const TemplateEngine::Context& context);

    // Вспомогательная функция для формирования HTTP-ответов
    std::string buildResponse(const std::string& status_code,
                              const std::string& content_type,
                              const std::string& body,
                              const std::map<std::string, std::string>& extra_headers = {});

    // Функции для управления cookies

    // Установка cookie
    void setCookie(std::ostringstream& response, const std::string& name, const std::string& value,
                  const std::string& path = "/", const std::string& expires = "",
                  bool httpOnly = true, bool secure = false, const std::string& sameSite = "Lax");

    // Удаление cookie
    void deleteCookie(std::ostringstream& response, const std::string& name,
                     const std::string& path = "/");

private:
    int port;
    bool verbose;
    TemplateEngine templateEngine; 

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

    // Добавленные функции
    void parseCookies(const std::string& cookieHeader, std::map<std::string, std::string>& cookies);
    std::string urlDecode(const std::string &value);
};

#endif // FLASKCPP_H
