#include "headers/FlaskCpp.h"

FlaskCpp::FlaskCpp(int port, bool verbose) : port(port), verbose(verbose) {
    if (verbose) {
        std::cout << "Initialized FlaskCpp on port: " << port << std::endl;
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
                if (verbose) {
                    std::cout << "Loaded template: " << filename << std::endl;
                }
            } else {
                std::cerr << "Failed to open template file: " << entry.path() << std::endl;
            }
        }
    }
}

void FlaskCpp::run() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
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
        return;
    }

    if (listen(serverSocket, 5) == -1) {
        std::cerr << "Listen failed." << std::endl;
        close(serverSocket);
        return;
    }

    std::cout << "Server is running on http://localhost:" << port << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket == -1) {
            std::cerr << "Failed to accept connection." << std::endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);

        std::thread(&FlaskCpp::handleClient, this, clientSocket, std::string(clientIP)).detach();
    }

    close(serverSocket);
}

std::string FlaskCpp::renderTemplate(const std::string& templateName, const TemplateEngine::Context& context) {
    return templateEngine.render(templateName, context);
}

std::string FlaskCpp::buildResponse(const std::string& status_code,
                                    const std::string& content_type,
                                    const std::string& body,
                                    const std::map<std::string, std::string>& extra_headers) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << "\r\n";
    response << "Content-Type: " << content_type;
    
    // Добавляем charset=utf-8 для текстовых типов контента
    if (content_type.find("text/") != std::string::npos || content_type.find("application/json") != std::string::npos) {
        response << "; charset=utf-8";
    }
    response << "\r\n";
    
    response << "Content-Length: " << body.size() << "\r\n";
    
    // Добавление дополнительных заголовков, если они есть
    for (const auto& header : extra_headers) {
        response << header.first << ": " << header.second << "\r\n";
    }
    
    response << "Connection: close\r\n\r\n";
    response << body;
    return response.str();
}

void FlaskCpp::setCookie(std::ostringstream& response, const std::string& name, const std::string& value,
                        const std::string& path, const std::string& expires,
                        bool httpOnly, bool secure, const std::string& sameSite) {
    response << "Set-Cookie: " << name << "=" << value;
    response << "; Path=" << path;
    if (!expires.empty()) {
        response << "; Expires=" << expires;
    }
    if (httpOnly) {
        response << "; HttpOnly";
    }
    if (secure) {
        response << "; Secure";
    }
    if (!sameSite.empty()) {
        response << "; SameSite=" << sameSite;
    }
    response << "\r\n";
}

void FlaskCpp::deleteCookie(std::ostringstream& response, const std::string& name,
                           const std::string& path) {
    response << "Set-Cookie: " << name << "=deleted";
    response << "; Path=" << path;
    response << "; Expires=Thu, 01 Jan 1970 00:00:00 GMT";
    response << "; HttpOnly\r\n";
}

void FlaskCpp::handleClient(int clientSocket, const std::string& clientIP) {
    try {
        std::string requestStr = readRequest(clientSocket);
        RequestData reqData;
        parseRequest(requestStr, reqData);

        std::cout << reqData.method << " " << reqData.path << " from " << clientIP << std::endl;

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
<html>
<head><title>404</title></head>
<body><h1>404 Not Found</h1></body>
</html>
)";
    response << "HTTP/1.1 404 Not Found\r\n"
             << "Content-Type: text/html\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    return response.str();
}

std::string FlaskCpp::generate500Error(const std::string& msg) {
    std::string body = "<h1>500 Internal Server Error</h1><p>" + msg + "</p>";
    std::ostringstream response;
    response << "HTTP/1.1 500 Internal Server Error\r\n"
             << "Content-Type: text/html\r\n"
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
