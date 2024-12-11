#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <sstream>

// Вспомогательные функции для управления cookies

// Функция для установки cookie
std::string setCookieResponse(const std::string& body, const std::string& name, const std::string& value,
                              const std::string& path = "/", const std::string& expires = "",
                              bool httpOnly = true, bool secure = false, const std::string& sameSite = "Lax");

// Функция для удаления cookie
std::string deleteCookieResponse(const std::string& body, const std::string& name,
                                 const std::string& path = "/");

#endif // UTILS_H
