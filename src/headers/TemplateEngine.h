#ifndef TEMPLATEENGINE_H
#define TEMPLATEENGINE_H

#include <string>
#include <map>
#include <variant>
#include <vector>
#include <regex>
#include <unordered_map>
#include <functional>
#include <mutex> // Для std::mutex и std::lock_guard

class TemplateEngine {
public:
    using ValueType = std::variant<std::string, bool, std::vector<std::map<std::string, std::string>>>;
    using Context = std::map<std::string, ValueType>;

    // Установка шаблона по имени
    void setTemplate(const std::string& name, const std::string& content);

    // Рендер шаблона
    std::string render(const std::string& templateName, const Context& context) const;

private:
    std::map<std::string, std::string> templates;

    std::string getTemplateContent(const std::string& name) const;

    // Рендер всего шаблона: обрабатываем extends, comments, include, if, for и переменные
    std::string renderTemplateContent(const std::string& content, const Context& context) const;

    // Вспомогательные функции
    bool evaluateCondition(const std::string& varName, const Context& context) const;
    std::string replaceVariables(const std::string& str, const Context& context) const;
    void renderLoop(const std::string& loopVar, const std::string& listName, const std::string& innerBlock, const Context& context, std::string& output) const;
    std::string processIf(const std::string& block, const Context& context) const;
    std::string processFor(const std::string& block, const Context& context) const;
    std::string processInclude(const std::string& block, const Context& context) const;
    std::string processExtends(const std::string& content, const Context& context, std::map<std::string, std::string>& childBlocks) const;
    std::map<std::string, std::string> extractBlocks(const std::string& content) const;
    std::string applyFilters(const std::string& value, const std::string& filter) const;
    std::string trim(const std::string& s) const;

    // Обработка комментариев
    std::string processComments(const std::string& content) const;

    // Кэширование включений
    struct IncludeCacheKey {
        std::string includeName;
        size_t contextHash;

        bool operator==(const IncludeCacheKey& other) const {
            return includeName == other.includeName && contextHash == other.contextHash;
        }
    };

    // Хэш-функция для IncludeCacheKey
    struct IncludeCacheKeyHash {
        std::size_t operator()(const IncludeCacheKey& key) const {
            return std::hash<std::string>()(key.includeName) ^ (std::hash<size_t>()(key.contextHash) << 1);
        }
    };

    mutable std::unordered_map<IncludeCacheKey, std::string, IncludeCacheKeyHash> includeCache;
    mutable std::mutex cacheMutex; // Для потокобезопасности

    // Функция для хэширования контекста
    size_t hashContext(const Context& context) const;
};

#endif // TEMPLATEENGINE_H
