#include "headers/FlaskCpp.h"
#include <cstdlib> // Для atoi
#include <csignal> // Для обработки сигналов
#include <thread>
#include <atomic>
#include <sys/select.h> // Для select
#include <fcntl.h>      // Для fcntl

// Конструктор
FlaskCpp::FlaskCpp(int port, bool verbose, bool enableHotReload, size_t minThreads, size_t maxThreads) 
    : port(port), verbose(verbose), enableHotReload(enableHotReload), running(false), threadPool(minThreads, maxThreads) {
    if (verbose) {
        std::cout << "Initialized FlaskCpp on port: " << port 
                  << (enableHotReload ? " with" : " without") << " hot_reload" << std::endl;
        std::cout << "ThreadPool initialized with minThreads=" << minThreads 
                  << " and maxThreads=" << maxThreads << "." << std::endl;
    }
}

void FlaskCpp::setTemplate(const std::string& name, const std::string& content) {
    templateEngine.setTemplate(name, content);
}

void FlaskCpp::route(const std::string& path, SimpleHandler handler) {
    routes[path] = [handler](const RequestData& req) {
        return handler(req);
    };
    if (verbose) {
        std::cout << "Route added: " << path << std::endl;
    }
}

void FlaskCpp::routeParam(const std::string& pattern, ComplexHandler handler) {
    paramRoutes.push_back({pattern, handler});
    if (verbose) {
        std::cout << "Param route added: " << pattern << std::endl;
    }
}

void FlaskCpp::loadTemplatesFromDirectory(const std::string& directoryPath) {
    namespace fs = std::filesystem;
    templatesDirectory = directoryPath;

    if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
        std::cerr << "Templates directory does not exist: " << directoryPath << std::endl;
        return;
    }

    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".html") {
            std::ifstream file(entry.path());
            if (file) {
                std::ostringstream ss;
                ss << file.rdbuf();
                std::string content = ss.str();
                std::string filename = entry.path().filename().string();
                setTemplate(filename, content);

                // Сохраняем временную метку файла
                templatesTimestamps[entry.path().string()] = fs::last_write_time(entry);

                if (verbose) {
                    std::cout << "Loaded template: " << filename << std::endl;
                }
            } else {
                std::cerr << "Failed to open template file: " << entry.path() << std::endl;
            }
        }
    }
}

void FlaskCpp::monitorTemplates() {
    namespace fs = std::filesystem;

    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Умеренный интервал проверки

        if (templatesDirectory.empty()) continue;

        for (const auto& entry : fs::directory_iterator(templatesDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".html") {
                std::string filePath = entry.path().string();
                auto currentTimestamp = fs::last_write_time(entry);

                // Если файл изменился, перезагрузим его
                if (templatesTimestamps.find(filePath) != templatesTimestamps.end()) {
                    if (templatesTimestamps[filePath] != currentTimestamp) {
                        std::ifstream file(entry.path());
                        if (file) {
                            std::ostringstream ss;
                            ss << file.rdbuf();
                            std::string content = ss.str();
                            std::string filename = entry.path().filename().string();
                            setTemplate(filename, content);

                            templatesTimestamps[filePath] = currentTimestamp;

                            if (verbose) {
                                std::cout << "Template reloaded: " << filename << std::endl;
                            }
                        }
                    }
                } else {
                    // Новый файл
                    templatesTimestamps[filePath] = currentTimestamp;
                }
            }
        }
    }
}

void FlaskCpp::runAsync() {
    if (running.load()) {
        std::cerr << "Server is already running." << std::endl;
        return;
    }

    running.store(true);

    // Запускаем поток мониторинга только если hot_reload включен
    if (enableHotReload) {
        hotReloadThread = std::thread(&FlaskCpp::monitorTemplates, this);
        if (verbose) {
            std::cout << "Hot reload is enabled. Monitoring templates for changes." << std::endl;
        }
    } else {
        if (verbose) {
            std::cout << "Hot reload is disabled." << std::endl;
        }
    }

    // Добавляем задачу запуска сервера в пул потоков с высоким приоритетом
    // Предполагаем, что приоритет 0 - самый высокий
    threadPool.enqueue(0, [this](){
        this->run();
    });
}

void FlaskCpp::run() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        running.store(false);
        return;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Bind failed." << std::endl;
        close(serverSocket);
        running.store(false);
        return;
    }

    if (listen(serverSocket, 100) == -1) { // Увеличиваем backlog для большей нагрузки
        std::cerr << "Listen failed." << std::endl;
        close(serverSocket);
        running.store(false);
        return;
    }

    if (verbose) {
        std::cout << "Server is running on http://localhost:" << port << std::endl;
    } else {
        std::cout << "Server started on port " << port << std::endl;
    }

    while (running.load()) { //  цикл для поддержки остановки сервера
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket == -1) {
            if (running.load()) { // Проверяем, не была ли остановка сервера
                std::cerr << "Failed to accept connection." << std::endl;
            }
            continue;
        }

        // Устанавливаем таймаут для чтения первого блока данных (например, 5 секунд)
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        // Читаем первые 4096 байт для определения метода запроса
        char buffer[4096];
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), MSG_PEEK);
        std::string requestSample;
        if (bytesRead > 0) {
            requestSample = std::string(buffer, bytesRead);
        }

        // Извлекаем метод запроса из первой строки
        std::string method = "GET"; // По умолчанию
        size_t firstLineEnd = requestSample.find("\r\n");
        if (firstLineEnd != std::string::npos) {
            std::string firstLine = requestSample.substr(0, firstLineEnd);
            std::istringstream iss(firstLine);
            iss >> method;
        }

        // Присваиваем приоритет на основе метода запроса
        int priority = 5; // Средний приоритет по умолчанию
        if (method == "GET") {
            priority = 1; // Высокий приоритет для GET
        } else if (method == "POST") {
            priority = 2; // Средний приоритет для POST
        } else if (method == "PUT" || method == "DELETE") {
            priority = 3; // Низкий приоритет для PUT и DELETE
        } else {
            priority = 4; // Очень низкий приоритет для остальных методов
        }

        if (verbose) {
            std::cout << "Request Method: " << method << " - Assigned Priority: " << priority << std::endl;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);

        // Добавляем обработку клиента в пул потоков с определённым приоритетом
        threadPool.enqueue(priority, [this, clientSocket, clientIP]() {
            this->handleClient(clientSocket, std::string(clientIP));
        });
    }

    close(serverSocket);
}

void FlaskCpp::stop() {
    if (!running.load()) return;

    running.store(false);

    // Создаем соединение с серверным сокетом, чтобы прервать блокировку accept
    int dummySocket = socket(AF_INET, SOCK_STREAM, 0);
    if(dummySocket != -1){
        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverAddr.sin_port = htons(port);
        connect(dummySocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
        close(dummySocket);
    }

    // Ожидаем завершения потока мониторинга
    if (enableHotReload && hotReloadThread.joinable()) {
        hotReloadThread.join();
    }

    // Останавливаем пул потоков
    threadPool.shutdown();

    if (verbose) {
        std::cout << "Server has been stopped." << std::endl;
    }
}

std::string FlaskCpp::renderTemplate(const std::string& templateName, const TemplateEngine::Context& context) {
    return templateEngine.render(templateName, context);
}

std::string FlaskCpp::buildResponse(const std::string& status_code,
                                    const std::string& content_type,
                                    const std::string& body,
                                    const std::vector<std::pair<std::string, std::string>>& extra_headers) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << "\r\n";
    response << "Content-Type: " << content_type;

    // Добавляем charset=utf-8 для текстовых типов контента
    if (content_type.find("text/") != std::string::npos || content_type.find("application/json") != std::string::npos) {
        response << "; charset=utf-8";
    }
    response << "\r\n";

    response << "Content-Length: " << body.size() << "\r\n";

    // Добавление дополнительных заголовков, включая несколько Set-Cookie
    for (const auto& header : extra_headers) {
        response << header.first << ": " << header.second << "\r\n";
    }

    response << "Connection: close\r\n\r\n";
    response << body;
    return response.str();
}

std::string FlaskCpp::setCookie(const std::string& name, const std::string& value,
                                const std::string& path, const std::string& expires,
                                bool httpOnly, bool secure, const std::string& sameSite) {
    std::ostringstream cookie;
    cookie << name << "=" << value;
    cookie << "; Path=" << path;
    if (!expires.empty()) {
        cookie << "; Expires=" << expires;
    }
    if (httpOnly) {
        cookie << "; HttpOnly";
    }
    if (secure) {
        cookie << "; Secure";
    }
    if (!sameSite.empty()) {
        cookie << "; SameSite=" << sameSite;
    }
    return cookie.str();
}

std::string FlaskCpp::deleteCookie(const std::string& name,
                                   const std::string& path) {
    std::ostringstream cookie;
    cookie << name << "=deleted";
    cookie << "; Path=" << path;
    cookie << "; Expires=Thu, 01 Jan 1970 00:00:00 GMT";
    cookie << "; HttpOnly";
    return cookie.str();
}

void FlaskCpp::handleClient(int clientSocket, const std::string& clientIP) {
    try {
        std::string requestStr = readRequest(clientSocket);
        RequestData reqData;
        parseRequest(requestStr, reqData);

        if (verbose) {
            std::cout << reqData.method << " " << reqData.path << " from " << clientIP << std::endl;
        }

        std::string response;
        {
            std::lock_guard<std::mutex> lock(routeMutex);

            ComplexHandler handler = nullptr;
            // Пытаемся найти точный маршрут
            auto it = routes.find(reqData.path);
            if (it != routes.end()) {
                handler = it->second;
            } else {
                // Проверяем маршруты с параметрами
                for (auto &pr : paramRoutes) {
                    if (matchParamRoute(reqData.path, pr.pattern, reqData.routeParams)) {
                        handler = pr.handler;
                        break;
                    }
                }
            }

            if (!handler) {
                // Проверим статические файлы
                if (!serveStaticFile(reqData, response)) {
                    response = generate404Error();
                }
            } else {
                response = handler(reqData);
            }
        }

        sendResponse(clientSocket, response);
        close(clientSocket);
    } catch (std::exception& e) {
        std::string response = generate500Error(e.what());
        sendResponse(clientSocket, response);
        close(clientSocket);
    } catch (...) {
        std::string response = generate500Error("Unknown error");
        sendResponse(clientSocket, response);
        close(clientSocket);
    }
}

std::string FlaskCpp::readRequest(int clientSocket) {
    std::string request;
    char buffer[4096];
    ssize_t bytesRead;
    // Читаем заголовки
    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), MSG_PEEK)) > 0) {
        std::string temp(buffer, bytesRead);
        size_t pos = temp.find("\r\n\r\n");
        if (pos != std::string::npos) {
            recv(clientSocket, buffer, pos+4, 0);
            request.append(buffer, pos+4);
            break;
        } else {
            recv(clientSocket, buffer, bytesRead, 0);
            request.append(buffer, bytesRead);
        }
    }

    // Проверим Content-Length для чтения тела
    std::string headers = request;
    size_t bodyPos = headers.find("\r\n\r\n");
    int contentLength = 0;
    if (bodyPos != std::string::npos) {
        std::string headerPart = headers.substr(0, bodyPos);
        std::istringstream iss(headerPart);
        std::string line;
        while (std::getline(iss, line)) {
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            if (line.find("Content-Length:") != std::string::npos) {
                std::string lenStr = line.substr(line.find(":")+1);
                lenStr.erase(0,lenStr.find_first_not_of(' '));
                contentLength = std::stoi(lenStr);
            }
        }

        if (contentLength > 0) {
            std::string body;
            body.resize(contentLength);
            int totalRead = 0;
            while (totalRead < contentLength) {
                ssize_t r = recv(clientSocket, &body[totalRead], contentLength - totalRead, 0);
                if (r <= 0) break;
                totalRead += r;
            }
            request.append(body);
        }
    }

    return request;
}

void FlaskCpp::parseRequest(const std::string& request, RequestData& reqData) {
    std::istringstream stream(request);
    std::string firstLine;
    std::getline(stream, firstLine);
    firstLine.erase(std::remove(firstLine.begin(), firstLine.end(), '\r'), firstLine.end());
    {
        std::istringstream fls(firstLine);
        fls >> reqData.method;
        std::string fullPath;
        fls >> fullPath;
        // Версию HTTP можно не обрабатывать подробно
    }

    // Заголовки
    std::string line;
    while (std::getline(stream, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        if (line.empty()) break;
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string val = line.substr(colonPos+1);
            while(!val.empty() && isspace((unsigned char)val.front())) val.erase(val.begin());
            reqData.headers[key] = val;
        }
    }

    // Остаток - тело
    {
        std::string all = request;
        size_t bodyPos = all.find("\r\n\r\n");
        if (bodyPos != std::string::npos) {
            reqData.body = all.substr(bodyPos+4);
        }
    }

    // Парсим query string
    {
        std::istringstream fls(firstLine);
        std::string tmp, fullPath;
        fls >> tmp >> fullPath;
        size_t questionMarkPos = fullPath.find('?');
        if (questionMarkPos != std::string::npos) {
            reqData.path = fullPath.substr(0, questionMarkPos);
            std::string queryString = fullPath.substr(questionMarkPos + 1);
            parseQueryString(queryString, reqData.queryParams);
        } else {
            reqData.path = fullPath;
        }
    }

    // Если POST и Content-Type: application/x-www-form-urlencoded, парсим formData
    if (reqData.method == "POST") {
        auto ctypeIt = reqData.headers.find("Content-Type");
        if (ctypeIt != reqData.headers.end() && ctypeIt->second.find("application/x-www-form-urlencoded") != std::string::npos) {
            parseQueryString(reqData.body, reqData.formData);
        }
    }

    // Парсинг cookies
    auto cookieIt = reqData.headers.find("Cookie");
    if (cookieIt != reqData.headers.end()) {
        parseCookies(cookieIt->second, reqData.cookies);
    }
}

void FlaskCpp::parseQueryString(const std::string& queryString, std::map<std::string, std::string>& queryParams) {
    std::istringstream stream(queryString);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        size_t equalSignPos = pair.find('=');
        std::string key, value;
        if (equalSignPos != std::string::npos) {
            key = pair.substr(0, equalSignPos);
            value = pair.substr(equalSignPos + 1);
        } else {
            key = pair;
        }
        queryParams[key] = urlDecode(value);
    }
}

bool FlaskCpp::matchParamRoute(const std::string& path, const std::string& pattern, std::map<std::string,std::string>& routeParams) {
    // Разбиваем path и pattern по '/'
    auto splitPath = [](const std::string&s){
        std::vector<std::string> parts;
        std::istringstream iss(s);
        std::string p;
        while(std::getline(iss, p, '/')) {
            if (!p.empty()) parts.push_back(p);
        }
        return parts;
    };

    auto pathParts = splitPath(path);
    auto patternParts = splitPath(pattern);
    if (pathParts.size() != patternParts.size()) return false;

    for (size_t i=0; i<pathParts.size(); i++) {
        if (patternParts[i].size()>2 && patternParts[i].front()=='<' && patternParts[i].back()=='>') {
            std::string paramName = patternParts[i].substr(1, patternParts[i].size()-2);
            routeParams[paramName] = pathParts[i];
        } else {
            if (patternParts[i] != pathParts[i]) return false;
        }
    }

    return true;
}

bool FlaskCpp::serveStaticFile(const RequestData& reqData, std::string& response) {
    if (reqData.path.rfind("/static/", 0) == 0) {
        std::string filename = reqData.path.substr(8); // Убираем /static/
        std::filesystem::path filePath = std::filesystem::current_path() / "static" / filename;
        if (std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath)) {
            std::string ext = filePath.extension().string();
#ifdef ENABLE_PHP
            if(ext == ".php"){
                // Поддержка PHP через php-cgi
                std::string phpOutput = executePHP(reqData, filePath);
                response = phpOutput;
                return true;
            }
#endif

            std::string ct = "text/plain";
            if (ext == ".html") ct = "text/html";
            else if (ext == ".css") ct = "text/css";
            else if (ext == ".js") ct = "application/javascript";
            else if (ext == ".json") ct = "application/json";
            else if (ext == ".png") ct = "image/png";
            else if (ext == ".jpg" || ext == ".jpeg") ct = "image/jpeg";
            else if (ext == ".gif") ct = "image/gif";

            std::ifstream f(filePath, std::ios::binary);
            if (!f) return false;
            std::ostringstream oss;
            oss << f.rdbuf();
            std::string body = oss.str();

            std::ostringstream resp;
            resp << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: " << ct << "\r\n"
                 << "Content-Length: " << body.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << body;

            response = resp.str();
            return true;
        }
    }
    return false;
}

void FlaskCpp::sendResponse(int clientSocket, const std::string& content) {
    send(clientSocket, content.c_str(), content.size(), 0);
}

std::string FlaskCpp::generate404Error() {
    std::ostringstream response;
    std::string body = R"(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <title>404 Not Found</title>
    <style>
        body {
            background-color: #f0f2f5;
            color: #333;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            text-align: center;
        }
        .container {
            background-color: #fff;
            padding: 40px 60px;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        h1 {
            font-size: 80px;
            margin-bottom: 20px;
            color: #e74c3c;
        }
        p {
            font-size: 24px;
            margin-bottom: 30px;
        }
        a {
            display: inline-block;
            padding: 12px 25px;
            background-color: #3498db;
            color: #fff;
            text-decoration: none;
            border-radius: 4px;
            font-size: 18px;
            transition: background-color 0.3s ease;
        }
        a:hover {
            background-color: #2980b9;
        }
        .illustration {
            margin-bottom: 30px;
        }
        @media (max-width: 600px) {
            .container {
                padding: 20px 30px;
            }
            h1 {
                font-size: 60px;
            }
            p {
                font-size: 20px;
            }
            a {
                font-size: 16px;
                padding: 10px 20px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="illustration">
            <!-- Можно добавить SVG или изображение здесь -->
            <svg width="100" height="100" viewBox="0 0 24 24" fill="#e74c3c" xmlns="http://www.w3.org/2000/svg">
                <path d="M12 0C5.371 0 0 5.371 0 12c0 6.629 5.371 12 12 12s12-5.371 12-12C24 5.371 18.629 0 12 0zm5.707 16.293L16.293 17.707 12 13.414 7.707 17.707 6.293 16.293 10.586 12 6.293 7.707 7.707 6.293 12 10.586 16.293 6.293 17.707 7.707 13.414 12 17.707z"/>
            </svg>
        </div>
        <h1>404</h1>
        <p>Упс! Страница, которую вы ищете, не найдена.</p>
        <a href="/">Вернуться на главную</a>
    </div>
</body>
</html>
)";
    response << "HTTP/1.1 404 Not Found\r\n"
             << "Content-Type: text/html; charset=UTF-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    return response.str();
}

std::string FlaskCpp::generate500Error(const std::string& msg) {
    std::ostringstream response;
    std::string body = R"(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <title>500 Internal Server Error</title>
    <style>
        body {
            background-color: #f8d7da;
            color: #721c24;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            text-align: center;
        }
        .container {
            background-color: #f5c6cb;
            padding: 40px 60px;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            max-width: 600px;
            margin: 20px;
        }
        h1 {
            font-size: 80px;
            margin-bottom: 20px;
            color: #c82333;
        }
        p {
            font-size: 24px;
            margin-bottom: 30px;
        }
        a {
            display: inline-block;
            padding: 12px 25px;
            background-color: #c82333;
            color: #fff;
            text-decoration: none;
            border-radius: 4px;
            font-size: 18px;
            transition: background-color 0.3s ease;
        }
        a:hover {
            background-color: #a71d2a;
        }
        .illustration {
            margin-bottom: 30px;
        }
        @media (max-width: 600px) {
            .container {
                padding: 20px 30px;
            }
            h1 {
                font-size: 60px;
            }
            p {
                font-size: 20px;
            }
            a {
                font-size: 16px;
                padding: 10px 20px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="illustration">
            <!-- SVG-иллюстрация для визуального эффекта -->
            <svg width="100" height="100" viewBox="0 0 24 24" fill="#c82333" xmlns="http://www.w3.org/2000/svg">
                <path d="M12 0C5.371 0 0 5.371 0 12c0 6.629 5.371 12 12 12s12-5.371 12-12C24 5.371 18.629 0 12 0zm5.707 16.293L16.293 17.707 12 13.414 7.707 17.707 6.293 16.293 10.586 12 6.293 7.707 7.707 6.293 12 10.586 16.293 6.293 17.707 7.707 13.414 12 17.707z"/>
            </svg>
        </div>
        <h1>500</h1>
        <p>Упс! Произошла внутренняя ошибка сервера.</p>
        <a href="/">Вернуться на главную</a>
    </div>
</body>
</html>
)";

    response << "HTTP/1.1 500 Internal Server Error\r\n"
             << "Content-Type: text/html; charset=UTF-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    return response.str();
}

// Реализация parseCookies
void FlaskCpp::parseCookies(const std::string& cookieHeader, std::map<std::string, std::string>& cookies) {
    std::istringstream stream(cookieHeader);
    std::string pair;
    while (std::getline(stream, pair, ';')) {
        size_t equalPos = pair.find('=');
        if (equalPos != std::string::npos) {
            std::string key = pair.substr(0, equalPos);
            std::string value = pair.substr(equalPos + 1);
            // Удаляем пробелы в начале ключа
            key.erase(0, key.find_first_not_of(' '));
            cookies[key] = urlDecode(value);
        }
    }
}

// Реализация urlDecode
std::string FlaskCpp::urlDecode(const std::string &value) {
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            std::string hex = value.substr(i + 1, 2);
            char decoded_char = static_cast<char>(std::stoi(hex, nullptr, 16));
            result += decoded_char;
            i += 2;
        } else if (value[i] == '+') {
            result += ' ';
        } else {
            result += value[i];
        }
    }
    return result;
}

#ifdef ENABLE_PHP
// Реализация executePHP через php-cgi с использованием popen
std::string FlaskCpp::executePHP(const RequestData& reqData, const std::filesystem::path& scriptPath) {
    // Команда для выполнения php-cgi
    std::string command = "php-cgi " + scriptPath.string();

    // Открываем процесс для чтения вывода PHP-скрипта
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return generate500Error("Failed to execute PHP script");
    }

    // Читаем вывод PHP-скрипта
    std::string phpOutput;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        phpOutput += buffer;
    }

    int returnCode = pclose(pipe);
    if (returnCode != 0) {
        return generate500Error("PHP script execution failed");
    }

    // Проверяем, начинается ли вывод PHP с "Status:"
    // Если да, извлекаем статус и удаляем его из вывода
    std::string statusLine = "HTTP/1.1 200 OK\r\n"; // По умолчанию
    size_t statusPos = phpOutput.find("Status:");
    if(statusPos != std::string::npos){
        size_t endLine = phpOutput.find("\r\n", statusPos);
        if(endLine != std::string::npos){
            statusLine = phpOutput.substr(statusPos, endLine - statusPos) + "\r\n";
            phpOutput.erase(statusPos, endLine - statusPos + 2); // Удаляем строку состояния из вывода
        }
    }

    // Формируем окончательный ответ
    std::string finalResponse = statusLine + phpOutput;
    return finalResponse;
}
#endif
