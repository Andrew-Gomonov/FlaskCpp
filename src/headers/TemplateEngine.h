#ifndef TEMPLATEENGINE_H
#define TEMPLATEENGINE_H

#include <string>
#include <map>
#include <variant>
#include <vector>

class TemplateEngine {
public:
    using ValueType = std::variant<std::string, bool, std::vector<std::map<std::string,std::string>>>;
    using Context = std::map<std::string, ValueType>;

    // Установка шаблона по имени
    void setTemplate(const std::string& name, const std::string& content);

    // Рендер шаблона
    std::string render(const std::string& templateName, const Context& context) const;

private:
    std::map<std::string, std::string> templates;

    std::string getTemplateContent(const std::string& name) const;

    // Рендер всего шаблона: обрабатываем include, if, for и переменные
    std::string renderTemplateContent(const std::string& content, const Context& context) const;
    
    // Вспомогательные функции
    bool evaluateCondition(const std::string& varName, const Context& context) const;
    std::string replaceVariables(const std::string& str, const Context& context) const;
    void renderLoop(const std::string& loopVar, const std::string& listName, const std::string& innerBlock, const Context& context, std::string& output) const;
    std::string processIf(const std::string& block, const Context& context) const;
    std::string processFor(const std::string& block, const Context& context) const;
    std::string processInclude(const std::string& block, const Context& context) const;
    std::string applyFilters(const std::string& value, const std::string& filter) const;

    std::string trim(const std::string& s) const;
};

#endif // TEMPLATEENGINE_H
