#include "headers/TemplateEngine.h"
#include <sstream>
#include <algorithm>
#include <cctype>

void TemplateEngine::setTemplate(const std::string& name, const std::string& content) {
    templates[name] = content;
}

std::string TemplateEngine::getTemplateContent(const std::string& name) const {
    auto it = templates.find(name);
    if (it != templates.end()) return it->second;
    return "";
}

std::string TemplateEngine::render(const std::string& templateName, const Context& context) const {
    std::string tpl = getTemplateContent(templateName);
    if (tpl.empty()) {
        return "Template not found: " + templateName;
    }
    return renderTemplateContent(tpl, context);
}

std::string TemplateEngine::renderTemplateContent(const std::string& content, const Context& context) const {
    // Порядок:
    // 1. include
    // 2. if
    // 3. for
    // 4. replace variables
    std::string result = content;

    // include
    result = processInclude(result, context);

    // if
    {
        size_t pos=0;
        while (true) {
            size_t ifPos = result.find("{% if ", pos);
            if (ifPos == std::string::npos) break;
            result = processIf(result, context);
            pos=0;
        }
    }

    // for
    {
        size_t pos=0;
        while (true) {
            size_t forPos = result.find("{% for ", pos);
            if (forPos == std::string::npos) break;
            result = processFor(result, context);
            pos=0;
        }
    }

    // variables
    result = replaceVariables(result, context);

    return result;
}

bool TemplateEngine::evaluateCondition(const std::string& varName, const Context& context) const {
    auto it = context.find(varName);
    if (it == context.end()) return false;
    if (std::holds_alternative<bool>(it->second)) {
        return std::get<bool>(it->second);
    } else if (std::holds_alternative<std::string>(it->second)) {
        return !std::get<std::string>(it->second).empty();
    } else if (std::holds_alternative<std::vector<std::map<std::string,std::string>>>(it->second)) {
        return !std::get<std::vector<std::map<std::string,std::string>>>(it->second).empty();
    }
    return false;
}

std::string TemplateEngine::replaceVariables(const std::string& str, const Context& context) const {
    std::string result = str;
    size_t startPos = 0;
    while (true) {
        size_t start = result.find("{{", startPos);
        if (start == std::string::npos) break;
        size_t end = result.find("}}", start);
        if (end == std::string::npos) break;

        std::string varExpr = trim(result.substr(start+2, end-(start+2)));
        size_t pipePos = varExpr.find('|');
        std::string varName = varExpr;
        std::string filter;
        if (pipePos != std::string::npos) {
            varName = trim(varExpr.substr(0, pipePos));
            filter = trim(varExpr.substr(pipePos+1));
        }

        std::string replacement;
        auto it = context.find(varName);
        if (it == context.end()) {
            // Попытка обработать вложенные переменные, например item.field
            size_t dotPos = varName.find('.');
            if (dotPos != std::string::npos) {
                std::string parent = varName.substr(0, dotPos);
                std::string child = varName.substr(dotPos + 1);
                auto parentIt = context.find(parent);
                if (parentIt != context.end() && std::holds_alternative<std::vector<std::map<std::string,std::string>>>(parentIt->second)) {
                    // В данном упрощённом варианте берем первый элемент
                    const auto& vec = std::get<std::vector<std::map<std::string,std::string>>>(parentIt->second);
                    if (!vec.empty()) {
                        auto childIt = vec[0].find(child);
                        if (childIt != vec[0].end()) {
                            replacement = childIt->second;
                        }
                    }
                }
            } else {
                replacement = "";
            }
        } else {
            if (std::holds_alternative<std::string>(it->second)) {
                replacement = std::get<std::string>(it->second);
            } else if (std::holds_alternative<bool>(it->second)) {
                replacement = std::get<bool>(it->second) ? "true" : "false";
            } else if (std::holds_alternative<std::vector<std::map<std::string,std::string>>>(it->second)) {
                replacement = "[object]";
            }
        }

        if (!filter.empty()) {
            replacement = applyFilters(replacement, filter);
        }

        result.replace(start, (end - start) + 2, replacement);
        startPos = start + replacement.size();
    }
    return result;
}

void TemplateEngine::renderLoop(const std::string& loopVar, const std::string& listName, const std::string& innerBlock, const Context& context, std::string& output) const {
    auto it = context.find(listName);
    if (it == context.end()) return;
    if (!std::holds_alternative<std::vector<std::map<std::string,std::string>>>(it->second)) return;

    const auto& vec = std::get<std::vector<std::map<std::string,std::string>>>(it->second);
    for (const auto& item : vec) {
        Context iterationContext = context;
        for (auto &kv : item) {
            iterationContext[loopVar + "." + kv.first] = kv.second;
        }
        output += renderTemplateContent(innerBlock, iterationContext);
    }
}

std::string TemplateEngine::processIf(const std::string& block, const Context& context) const {
    size_t ifPos = block.find("{% if ");
    if (ifPos == std::string::npos) return block;
    size_t ifEnd = block.find("%}", ifPos);
    if (ifEnd == std::string::npos) return block;

    std::string condVar = trim(block.substr(ifPos+6, ifEnd-(ifPos+6)));
    bool condition = evaluateCondition(condVar, context);

    size_t endifPos = block.find("{% endif %}", ifEnd);
    if (endifPos == std::string::npos) return block;

    std::string inside = block.substr(ifEnd+2, endifPos-(ifEnd+2));
    size_t elsePos = inside.find("{% else %}");

    std::string chosen;
    if (condition) {
        // часть до else или вся строка если нет else
        if (elsePos != std::string::npos) {
            chosen = inside.substr(0, elsePos);
        } else {
            chosen = inside;
        }
    } else {
        // если есть else
        if (elsePos != std::string::npos) {
            size_t elseEnd = elsePos + std::string("{% else %}").size();
            chosen = inside.substr(elseEnd);
        } else {
            chosen = "";
        }
    }

    std::string result = block;
    result.replace(ifPos, (endifPos - ifPos) + 11, chosen);
    return result;
}

std::string TemplateEngine::processFor(const std::string& block, const Context& context) const {
    size_t forPos = block.find("{% for ");
    if (forPos == std::string::npos) return block;
    size_t forEnd = block.find("%}", forPos);
    if (forEnd == std::string::npos) return block;

    std::string forStmt = trim(block.substr(forPos+7, forEnd-(forPos+7)));
    std::istringstream iss(forStmt);
    std::string varName, inKeyword, listName;
    iss >> varName >> inKeyword >> listName;
    if (inKeyword != "in") return block;

    size_t endforPos = block.find("{% endfor %}", forEnd);
    if (endforPos == std::string::npos) return block;

    std::string innerBlock = block.substr(forEnd+2, endforPos-(forEnd+2));
    std::string renderedLoop;
    renderLoop(varName, listName, innerBlock, context, renderedLoop);

    std::string result = block;
    result.replace(forPos, (endforPos - forPos)+11, renderedLoop);
    return result;
}

std::string TemplateEngine::processInclude(const std::string& block, const Context& context) const {
    std::string result = block;
    size_t pos=0;
    while (true) {
        size_t incPos = result.find("{% include ", pos);
        if (incPos == std::string::npos) break;
        size_t endPos = result.find("%}", incPos);
        if (endPos == std::string::npos) break;
        std::string incStmt = trim(result.substr(incPos+11, endPos-(incPos+11)));
        if (incStmt.size()<2 || incStmt.front()!='"' || incStmt.back()!='"') {
            pos = endPos+2;
            continue;
        }
        std::string incName = incStmt.substr(1, incStmt.size()-2);
        std::string incContent = getTemplateContent(incName);
        if (incContent.empty()) {
            // Если включаемый шаблон не найден, оставить как есть
            pos = endPos+2;
            continue;
        }
        std::string replacement = renderTemplateContent(incContent, context);
        result.replace(incPos, (endPos - incPos)+2, replacement);
        pos = incPos + replacement.size();
    }
    return result;
}

std::string TemplateEngine::applyFilters(const std::string& value, const std::string& filter) const {
    if (filter == "upper") {
        std::string up = value;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        return up;
    } else if (filter == "lower") {
        std::string low = value;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        return low;
    } else if (filter == "escape") {
        std::string esc;
        for (auto c : value) {
            if (c == '<') esc += "&lt;";
            else if (c == '>') esc += "&gt;";
            else if (c == '&') esc += "&amp;";
            else if (c == '"') esc += "&quot;";
            else esc += c;
        }
        return esc;
    }
    return value;
}

std::string TemplateEngine::trim(const std::string& s) const {
    std::string r = s;
    r.erase(r.begin(), std::find_if(r.begin(), r.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    r.erase(std::find_if(r.rbegin(), r.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), r.end());
    return r;
}
