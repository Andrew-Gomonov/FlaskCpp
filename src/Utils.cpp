#include "headers/Utils.h"
#include <sstream>

// Функция для установки cookie
std::string setCookieResponse(const std::string& body, const std::string& name, const std::string& value,
                              const std::string& path, const std::string& expires,
                              bool httpOnly, bool secure, const std::string& sameSite) {
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/html\r\n";

    // Формируем заголовок Set-Cookie
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

    response << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    return response.str();
}

// Функция для удаления cookie
std::string deleteCookieResponse(const std::string& body, const std::string& name,
                                 const std::string& path) {
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/html\r\n"
             << "Set-Cookie: " << name << "=deleted"
             << "; Path=" << path
             << "; Expires=Thu, 01 Jan 1970 00:00:00 GMT"
             << "; HttpOnly\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    return response.str();
}
