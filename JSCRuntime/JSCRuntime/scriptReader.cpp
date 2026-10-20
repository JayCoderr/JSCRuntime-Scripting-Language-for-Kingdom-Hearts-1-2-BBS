#include "scriptReader.h"
#include "overlay.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <Windows.h>
#include <string>
#include <cctype>
#include <functional>
#include <cstdlib>
#include <cmath>
#include <limits>
#include "DebugFunctionAddress.h"

bool CreateRectangle(const char* imageSource, float x, float y, float width, float height, float alpha, int sort, const char* Parent);
bool CreateText(const char* text, void (*function)(), float x, float y, float fontSize, float alpha, int sort);
std::vector<ScriptThread> activeThreads;
ScriptThread* currentThread = nullptr;
static bool alreadyClearedUi = false;
ScriptVariable variable;

bool ScriptReader::Variable(
    const std::string& operation,
    ScriptVariable::Type type,
    const std::string& name,
    std::string value,
    void* result)
{
    auto trim = [](std::string& text)
        {
            size_t start = text.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) { text.clear(); return; }
            size_t end = text.find_last_not_of(" \t\r\n");
            text = text.substr(start, end - start + 1);
        };

    std::string variableName = name;
    trim(variableName);

    // Parse the root variable and every quoted array key.
    std::string rootName;
    std::vector<std::string> arrayKeys;

    size_t firstBracket = variableName.find('[');

    if (firstBracket != std::string::npos)
    {
        rootName = variableName.substr(0, firstBracket);
        trim(rootName);

        size_t prefix = rootName.rfind('.');
        if (prefix != std::string::npos)
            rootName = rootName.substr(prefix + 1);

        size_t pos = firstBracket;

        while (pos < variableName.size())
        {
            while (pos < variableName.size() &&
                std::isspace(static_cast<unsigned char>(variableName[pos])))
                ++pos;

            if (pos >= variableName.size() || variableName[pos] != '[')
                break;

            ++pos;

            while (pos < variableName.size() &&
                std::isspace(static_cast<unsigned char>(variableName[pos])))
                ++pos;

            if (pos >= variableName.size() ||
                (variableName[pos] != '"' && variableName[pos] != '\''))
            {
                std::cout << "[ScriptReader] Array keys must be quoted: "
                    << name << std::endl;
                return false;
            }

            char quote = variableName[pos++];
            size_t keyStart = pos;

            while (pos < variableName.size() && variableName[pos] != quote)
                ++pos;

            if (pos >= variableName.size())
            {
                std::cout << "[ScriptReader] Unterminated array key: "
                    << name << std::endl;
                return false;
            }

            std::string key = variableName.substr(keyStart, pos - keyStart);
            ++pos;

            while (pos < variableName.size() &&
                std::isspace(static_cast<unsigned char>(variableName[pos])))
                ++pos;

            if (pos >= variableName.size() || variableName[pos] != ']')
            {
                std::cout << "[ScriptReader] Invalid array path: "
                    << name << std::endl;
                return false;
            }

            ++pos;
            arrayKeys.push_back(std::move(key));
        }

        while (pos < variableName.size() &&
            std::isspace(static_cast<unsigned char>(variableName[pos])))
            ++pos;

        if (pos != variableName.size() ||
            rootName.empty() || arrayKeys.empty())
        {
            std::cout << "[ScriptReader] Invalid array path: "
                << name << std::endl;
            return false;
        }
    }
    else
    {
        rootName = variableName;

        size_t prefix = rootName.rfind('.');
        if (prefix != std::string::npos)
            rootName = rootName.substr(prefix + 1);

        trim(rootName);
    }

    if (rootName.empty())
        return false;

    auto parseValue = [&](ScriptVariable& variable) -> bool
        {
            variable.type = type;

            try
            {
                switch (type)
                {
                case ScriptVariable::BOOL:
                    if (value != "true" && value != "false")
                        return false;
                    variable.boolValue = (value == "true");
                    return true;

                case ScriptVariable::INT:
                {
                    size_t parsed = 0;
                    int number = std::stoi(value, &parsed);
                    if (parsed != value.size())
                        return false;
                    variable.intValue = number;
                    return true;
                }

                case ScriptVariable::FLOAT:
                {
                    std::string numeric = value;
                    if (!numeric.empty() &&
                        (numeric.back() == 'f' || numeric.back() == 'F'))
                        numeric.pop_back();

                    size_t parsed = 0;
                    float number = std::stof(numeric, &parsed);
                    if (parsed != numeric.size())
                        return false;

                    variable.floatValue = number;
                    return true;
                }

                case ScriptVariable::STRING:
                    variable.stringValue = value;
                    return true;

                case ScriptVariable::ARRAY:
                    if (value != "[]")
                        return false;

                    variable.arrayValue =
                        std::make_shared<std::unordered_map<std::string, ScriptVariable>>();
                    return true;

                case ScriptVariable::UNDEFINED:
                    return true;

                default:
                    return false;
                }
            }
            catch (...)
            {
                return false;
            }
        };

    // Resolve the root variable.
    auto rootIt = variables.find(rootName);

    if (rootIt == variables.end())
    {
        if (operation == "Create" && arrayKeys.empty())
        {
            ScriptVariable variable{};

            if (!parseValue(variable))
                return false;

            variables.emplace(rootName, std::move(variable));
            RefreshUIElements();
            return true;
        }

        std::cout << "[ScriptReader] Variable or array not found: "
            << rootName << std::endl;
        return false;
    }

    // Traverse every intermediate array.
    ScriptVariable* target = &rootIt->second;

    for (size_t i = 0; i < arrayKeys.size(); ++i)
    {
        if (target->type != ScriptVariable::ARRAY || !target->arrayValue)
        {
            std::cout << "[ScriptReader] Path component is not an array: "
                << arrayKeys[i] << std::endl;
            return false;
        }

        auto& entries = *target->arrayValue;
        auto entryIt = entries.find(arrayKeys[i]);

        const bool isFinal = (i == arrayKeys.size() - 1);

        if (isFinal)
        {
            if (operation == "Create")
            {
                if (entryIt != entries.end())
                    return false;

                ScriptVariable entry{};
                if (!parseValue(entry))
                    return false;

                entries.emplace(arrayKeys[i], std::move(entry));
                RefreshUIElements();
                return true;
            }

            if (operation == "Get")
            {
                if (entryIt == entries.end() ||
                    entryIt->second.type == ScriptVariable::UNDEFINED ||
                    result == nullptr)
                    return false;

                ScriptVariable& entry = entryIt->second;

                if (type == ScriptVariable::FLOAT &&
                    entry.type == ScriptVariable::INT)
                {
                    *static_cast<float*>(result) =
                        static_cast<float>(entry.intValue);
                    return true;
                }

                if (entry.type != type)
                    return false;

                switch (type)
                {
                case ScriptVariable::BOOL:
                    *static_cast<bool*>(result) = entry.boolValue;
                    return true;

                case ScriptVariable::INT:
                    *static_cast<int*>(result) = entry.intValue;
                    return true;

                case ScriptVariable::FLOAT:
                    *static_cast<float*>(result) = entry.floatValue;
                    return true;

                case ScriptVariable::STRING:
                    *static_cast<std::string*>(result) = entry.stringValue;
                    return true;

                default:
                    return false;
                }
            }

            if (operation == "Set")
            {
                if (type == ScriptVariable::UNDEFINED)
                {
                    if (entryIt != entries.end())
                        entries.erase(entryIt);

                    RefreshUIElements();
                    return true;
                }

                ScriptVariable entry{};
                if (!parseValue(entry))
                    return false;

                entries[arrayKeys[i]] = std::move(entry);
                RefreshUIElements();
                return true;
            }

            return false;
        }

        // Intermediate components must already exist.
        if (entryIt == entries.end() ||
            entryIt->second.type != ScriptVariable::ARRAY ||
            !entryIt->second.arrayValue)
        {
            std::cout << "[ScriptReader] Nested array not found: "
                << arrayKeys[i] << std::endl;
            return false;
        }

        target = &entryIt->second;
    }

    // Ordinary scalar or root-array operations.
    auto root = variables.find(rootName);

    if (operation == "Create")
    {
        if (root != variables.end())
            return false;

        ScriptVariable variable{};
        if (!parseValue(variable))
            return false;

        variables.emplace(rootName, std::move(variable));
        RefreshUIElements();
        return true;
    }

    if (operation == "Get")
    {
        std::string numericValue = rootName;

        if (!numericValue.empty() &&
            (numericValue.back() == 'f' || numericValue.back() == 'F'))
            numericValue.pop_back();

        try
        {
            size_t parsed = 0;
            float number = std::stof(numericValue, &parsed);

            if (parsed == numericValue.size() &&
                (type == ScriptVariable::INT || type == ScriptVariable::FLOAT))
            {
                if (!result)
                    return false;

                if (type == ScriptVariable::INT)
                {
                    int intNumber = std::stoi(numericValue, &parsed);
                    if (parsed != numericValue.size())
                        return false;

                    *static_cast<int*>(result) = intNumber;
                }
                else
                {
                    *static_cast<float*>(result) = number;
                }

                return true;
            }
        }
        catch (...)
        {
        }

        if (root == variables.end() || !result)
            return false;

        ScriptVariable& variable = root->second;

        if (type == ScriptVariable::FLOAT &&
            variable.type == ScriptVariable::INT)
        {
            *static_cast<float*>(result) =
                static_cast<float>(variable.intValue);
            return true;
        }

        if (variable.type != type)
            return false;

        switch (type)
        {
        case ScriptVariable::BOOL:
            *static_cast<bool*>(result) = variable.boolValue;
            return true;

        case ScriptVariable::INT:
            *static_cast<int*>(result) = variable.intValue;
            return true;

        case ScriptVariable::FLOAT:
            *static_cast<float*>(result) = variable.floatValue;
            return true;

        case ScriptVariable::STRING:
            *static_cast<std::string*>(result) = variable.stringValue;
            return true;

        default:
            return false;
        }
    }

    if (operation == "Set")
    {
        if (root == variables.end())
            return false;

        ScriptVariable& variable = root->second;

        if (variable.type != type)
            return false;

        if (!parseValue(variable))
            return false;

        RefreshUIElements();
        return true;
    }

    std::cout << "[ScriptReader] Invalid variable operation: "
        << operation << std::endl;

    return false;
}

bool ScriptReader::ExecuteForLoop(
    const std::string& initializer,
    const std::string& condition,
    const std::string& increment,
    const std::vector<ScriptCommand>& body,
    unsigned long long threadId,
    size_t commandIndex)
{
    auto trim = [](std::string& value)
        {
            const size_t start = value.find_first_not_of(" \t\r\n");
            const size_t end = value.find_last_not_of(" \t\r\n");

            value = start == std::string::npos
                ? ""
                : value.substr(start, end - start + 1);
        };

    std::string variableName;

    // Parse and apply the initializer.
    if (!initializer.empty())
    {
        const size_t assign = initializer.find('=');

        if (assign == std::string::npos ||
            (assign > 0 && initializer[assign - 1] == '=') ||
            (assign + 1 < initializer.size() &&
                initializer[assign + 1] == '='))
        {
            return false;
        }

        variableName = initializer.substr(0, assign);
        std::string initialValue = initializer.substr(assign + 1);

        trim(variableName);
        trim(initialValue);

        if (variableName.empty() || initialValue.empty())
            return false;

        int initialNumber = 0;

        try
        {
            size_t parsed = 0;
            initialNumber = std::stoi(initialValue, &parsed);

            if (parsed != initialValue.size())
                return false;
        }
        catch (...)
        {
            return false;
        }

        if (!ExecuteVariableDeclaration(
            variableName + " = " + std::to_string(initialNumber),
            threadId,
            commandIndex))
        {
            return false;
        }
    }

    // Validate the complete condition using the shared evaluator.
    if (!condition.empty())
    {
        bool result = false;

        if (!EvaluateCondition(condition, result))
            return false;
    }

    // Validate that the increment targets the loop variable.
    if (!increment.empty())
    {
        std::string incrementVariable;
        size_t op = increment.find("++");

        if (op == std::string::npos)
            op = increment.find("--");

        if (op == std::string::npos)
            op = increment.find("+=");

        if (op == std::string::npos)
            op = increment.find("-=");

        if (op == std::string::npos)
            return false;

        incrementVariable = increment.substr(0, op);
        trim(incrementVariable);

        if (incrementVariable.empty())
            return false;

        if (variableName.empty())
        {
            variableName = incrementVariable;
        }
        else if (incrementVariable != variableName)
        {
            return false;
        }
    }

    // Find the owning thread.
    auto threadIt = std::find_if(
        activeThreads.begin(),
        activeThreads.end(),
        [threadId](const ScriptThread& thread)
        {
            return thread.threadId == threadId;
        });

    if (threadIt == activeThreads.end())
        return false;

    // Save the loop for resumable execution.
    ScriptExecutionState state;
    state.commands = body;
    state.commandIndex = 0;
    state.isForLoop = true;
    state.initializer = initializer;
    state.condition = condition;
    state.increment = increment;
    state.iterations = 0;
    state.checkForCondition = true;

    threadIt->executionStack.push_back(std::move(state));
    return true;
}

std::string ScriptReader::RemoveComments(
    const std::string& source)
{
    std::string cleanedSource;

    cleanedSource.reserve(
        source.size()
    );

    bool insideString = false;
    size_t i = 0;

    while (i < source.size())
    {
        char c = source[i];

        if (c == '"')
        {
            insideString = !insideString;

            cleanedSource += c;
            ++i;

            continue;
        }

        if (!insideString)
        {
            // =================================================
            // // COMMENT
            // =================================================

            if (c == '/' &&
                i + 1 < source.size() &&
                source[i + 1] == '/')
            {
                i += 2;

                while (
                    i < source.size() &&
                    source[i] != '\n')
                {
                    ++i;
                }

                continue;
            }

            // =================================================
            // /* BLOCK COMMENT */
            // =================================================

            if (c == '/' &&
                i + 1 < source.size() &&
                source[i + 1] == '*')
            {
                i += 2;

                while (i + 1 < source.size())
                {
                    if (
                        source[i] == '*' &&
                        source[i + 1] == '/')
                    {
                        i += 2;
                        break;
                    }

                    if (source[i] == '\n')
                    {
                        cleanedSource += '\n';
                    }

                    ++i;
                }

                continue;
            }

            // =================================================
            // -- COMMENT
            // =================================================

            if (c == '<!--' &&
                i + 1 < source.size() &&
                source[i + 1] == '-->')
            {
                i += 2;

                while (
                    i < source.size() &&
                    source[i] != '\n')
                {
                    ++i;
                }

                continue;
            }
        }

        cleanedSource += c;
        ++i;
    }

    return cleanedSource;
}

bool ScriptReader::Load(const char* filename)
{
    std::cout
        << "[ScriptReader][Load] Called for: "
        << filename
        << " | Instance: " << this
        << " | Variables before clear: " << variables.size()
        << " | Functions before clear: " << functions.size()
        << " | Active threads before clear: " << activeThreads.size()
        << std::endl;

    functions.clear();
    variables.clear();
    activeThreads.clear();
    uiElements.clear();

    std::cout
        << "[ScriptReader][Load] Containers cleared"
        << " | Instance: " << this
        << std::endl;

    std::ifstream file(filename);

    if (!file.is_open())
    {
        std::cout
            << "[ScriptReader] Failed to open: "
            << filename
            << std::endl;

        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string source = buffer.str();
    std::string cleanedSource = RemoveComments(source);

    // ========================================================
    // PARSE GLOBAL VARIABLES + FUNCTIONS
    // ========================================================

    size_t position = 0;

    while (position < cleanedSource.size())
    {
        // ====================================================
        // SKIP WHITESPACE
        // ====================================================

        while (
            position < cleanedSource.size() &&
            std::isspace(static_cast<unsigned char>(cleanedSource[position])))
        {
            ++position;
        }

        if (position >= cleanedSource.size())
            break;

        // ====================================================
        // FIND NEXT SYNTAX MARKERS
        // ====================================================

        size_t semicolon = cleanedSource.find(';', position);
        size_t openParen = cleanedSource.find('(', position);
        size_t openBrace = cleanedSource.find('{', position);

        // ====================================================
        // GLOBAL VARIABLE / ASSIGNMENT
        // ====================================================

        if (
            semicolon != std::string::npos &&
            (openParen == std::string::npos || semicolon < openParen) &&
            (openBrace == std::string::npos || semicolon < openBrace))
        {
            std::string declaration = cleanedSource.substr(position, semicolon - position);

            // ================================================
            // EXECUTE GLOBAL VARIABLE
            // ================================================

            if (!ExecuteVariableDeclaration(declaration, 0, 0))
            {
                std::cout
                    << "[ScriptReader] Failed to create global variable: "
                    << declaration
                    << std::endl;

                return false;
            }

            std::cout
                << "[ScriptReader] Global variable: "
                << declaration
                << std::endl;

            position = semicolon + 1;
            continue;
        }

        // ====================================================
        // PARSE FUNCTION
        // ====================================================

        if (openParen == std::string::npos || openBrace == std::string::npos)
            break;

        if (openParen > openBrace)
        {
            position = openBrace + 1;
            continue;
        }

        std::string functionName = cleanedSource.substr(position, openParen - position);

        while (
            !functionName.empty() &&
            std::isspace(static_cast<unsigned char>(functionName.back())))
        {
            functionName.pop_back();
        }

        size_t nameStart = 0;

        while (
            nameStart < functionName.size() &&
            std::isspace(static_cast<unsigned char>(functionName[nameStart])))
        {
            ++nameStart;
        }

        functionName = functionName.substr(nameStart);

        // ====================================================
        // FIND FUNCTION BODY
        // ====================================================

        size_t closeParen = cleanedSource.find(')', openParen);

        if (closeParen == std::string::npos)
        {
            std::cout
                << "[ScriptReader] Missing ')' in function: "
                << functionName
                << std::endl;

            return false;
        }

        size_t bodyStart = cleanedSource.find('{', closeParen);

        if (bodyStart == std::string::npos)
        {
            std::cout
                << "[ScriptReader] Missing function body for: "
                << functionName
                << std::endl;

            return false;
        }

        // ====================================================
        // MATCH FUNCTION BRACES
        // ====================================================

        int depth = 0;
        size_t bodyEnd = bodyStart;

        for (; bodyEnd < cleanedSource.size(); ++bodyEnd)
        {
            if (cleanedSource[bodyEnd] == '{')
            {
                ++depth;
            }
            else if (cleanedSource[bodyEnd] == '}')
            {
                --depth;

                if (depth == 0)
                    break;
            }
        }

        if (depth != 0)
        {
            std::cout
                << "[ScriptReader] Missing closing brace for: "
                << functionName
                << std::endl;

            return false;
        }

        // ========================================================
        // STORE FUNCTION + PARAMETERS
        // ========================================================

        std::string body = cleanedSource.substr(
            bodyStart + 1,
            bodyEnd - bodyStart - 1
        );

        ScriptFunction function;
        function.name = functionName;
        function.body = body;

        // Extract parameters from the function signature.
        std::string parameterList = cleanedSource.substr(
            openParen + 1,
            closeParen - openParen - 1
        );

        std::stringstream parameterStream(parameterList);
        std::string parameter;

        while (std::getline(parameterStream, parameter, ','))
        {
            size_t first = parameter.find_first_not_of(" \t\r\n");
            size_t last = parameter.find_last_not_of(" \t\r\n");

            if (first == std::string::npos)
                continue;

            parameter = parameter.substr(first, last - first + 1);

            function.parameters.push_back(parameter);
        }

        functions[functionName] = function;

        std::cout
            << "[ScriptReader] Found function: "
            << functionName
            << std::endl;

        position = bodyEnd + 1;
    }

    std::cout
        << "[ScriptReader] Loaded "
        << functions.size()
        << " functions and "
        << variables.size()
        << " global variables from "
        << filename
        << std::endl;

    return true;
}

bool ScriptReader::ExecuteVariableDeclaration(
    const std::string& declaration,
    unsigned long long threadId,
    size_t commandIndex
)
{
    auto trim = [](std::string& text)
        {
            size_t start = text.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
            {
                text.clear();
                return;
            }

            size_t end = text.find_last_not_of(" \t\r\n");
            text = text.substr(start, end - start + 1);
        };

    // ========================================================
    // VARIABLE PATH NORMALIZATION AND LIVE ALIAS RESOLUTION
    // ========================================================
    auto normalizeVariablePath = [&](std::string path) -> std::string
        {
            trim(path);

            size_t bracket = path.find('[');

            std::string root = bracket == std::string::npos
                ? path
                : path.substr(0, bracket);

            std::string suffix = bracket == std::string::npos
                ? ""
                : path.substr(bracket);

            size_t dot = root.rfind('.');

            if (dot != std::string::npos)
                root = root.substr(dot + 1);

            return root + suffix;
        };

    auto resolveVariableAlias = [&](const std::string& rawName) -> std::string
        {
            std::string resolved = normalizeVariablePath(rawName);

            if (currentThreadIndex < 0 ||
                currentThreadIndex >= static_cast<int>(activeThreads.size()))
            {
                return resolved;
            }

            const auto& aliases =
                activeThreads[currentThreadIndex].parameterAliases;

            for (size_t depth = 0; depth <= aliases.size(); ++depth)
            {
                size_t bracket = resolved.find('[');

                std::string root = bracket == std::string::npos
                    ? resolved
                    : resolved.substr(0, bracket);

                std::string suffix = bracket == std::string::npos
                    ? ""
                    : resolved.substr(bracket);

                auto aliasIt = aliases.find(root);

                if (aliasIt == aliases.end())
                    break;

                resolved = normalizeVariablePath(aliasIt->second) + suffix;
            }

            return normalizeVariablePath(resolved);
        };

    std::string name;
    std::string value;
    std::string op = "=";
    size_t operatorPos = std::string::npos;

    for (const char* candidate : { "+=", "-=", "*=", "/=", "++", "--" })
    {
        size_t found = declaration.find(candidate);

        if (found != std::string::npos &&
            (operatorPos == std::string::npos || found < operatorPos))
        {
            operatorPos = found;
            op = candidate;
        }
    }

    if (operatorPos != std::string::npos)
    {
        name = declaration.substr(0, operatorPos);

        value = (op == "++" || op == "--")
            ? "1"
            : declaration.substr(operatorPos + 2);
    }
    else
    {
        operatorPos = declaration.find('=');

        if (operatorPos == std::string::npos)
        {
            std::cout << "[ScriptReader] Invalid variable assignment: "
                << declaration << std::endl;
            return false;
        }

        name = declaration.substr(0, operatorPos);
        value = declaration.substr(operatorPos + 1);
    }

    trim(name);
    trim(value);

    if (name.empty() || value.empty())
        return false;

    // Resolve the destination before determining its type or operation.
    name = resolveVariableAlias(name);

    const bool arrayEntry = name.find('[') != std::string::npos;
    const bool arithmetic = op != "=";

    // ========================================================
    // ARRAY DECLARATIONS
    // ========================================================
    if (value == "[]")
    {
        if (op != "=")
            return false;

        return Variable("Create", ScriptVariable::ARRAY, name, value, nullptr);
    }

    // ========================================================
    // NUMERIC EXPRESSION EVALUATOR
    // Supports scalar variables, nested array entries,
    // parentheses, unary +/- and standard operator precedence.
    // ========================================================
    auto evaluateExpression = [&](const std::string& expression, double& result) -> bool
        {
            size_t pos = 0;

            auto skipSpaces = [&]()
                {
                    while (pos < expression.size() &&
                        std::isspace(static_cast<unsigned char>(expression[pos])))
                    {
                        ++pos;
                    }
                };


            auto getNumericVariable = [&](const std::string& rawName, double& out) -> bool
                {
                    size_t firstBracket = rawName.find('[');

                    // Scalar variable.
                    if (firstBracket == std::string::npos)
                    {
                        std::string variableName = resolveVariableAlias(rawName);
                        auto it = variables.find(variableName);

                        if (it == variables.end())
                            return false;

                        if (it->second.type == ScriptVariable::INT)
                        {
                            out = it->second.intValue;
                            return true;
                        }

                        if (it->second.type == ScriptVariable::FLOAT)
                        {
                            out = it->second.floatValue;
                            return true;
                        }

                        return false;
                    }

                    // Nested array entry.
                    std::string rootName = resolveVariableAlias(
                        rawName.substr(0, firstBracket));

                    auto rootIt = variables.find(rootName);

                    if (rootIt == variables.end() ||
                        rootIt->second.type != ScriptVariable::ARRAY ||
                        !rootIt->second.arrayValue)
                    {
                        return false;
                    }

                    ScriptVariable* target = &rootIt->second;
                    size_t bracketPos = firstBracket;

                    while (bracketPos < rawName.size())
                    {
                        if (rawName[bracketPos] != '[')
                            return false;

                        // Find the matching ']' while respecting quoted keys.
                        size_t closeBracket = std::string::npos;
                        char quote = '\0';

                        for (size_t i = bracketPos + 1; i < rawName.size(); ++i)
                        {
                            char c = rawName[i];

                            if (quote != '\0')
                            {
                                if (c == quote && (i == 0 || rawName[i - 1] != '\\'))
                                    quote = '\0';
                            }
                            else if (c == '"' || c == '\'')
                            {
                                quote = c;
                            }
                            else if (c == ']')
                            {
                                closeBracket = i;
                                break;
                            }
                        }

                        if (closeBracket == std::string::npos)
                            return false;

                        std::string key = rawName.substr(
                            bracketPos + 1,
                            closeBracket - bracketPos - 1);

                        trim(key);

                        if (key.size() < 2 ||
                            !((key.front() == '"' && key.back() == '"') ||
                                (key.front() == '\'' && key.back() == '\'')))
                        {
                            return false;
                        }

                        key = key.substr(1, key.size() - 2);

                        if (target->type != ScriptVariable::ARRAY ||
                            !target->arrayValue)
                        {
                            return false;
                        }

                        auto entryIt = target->arrayValue->find(key);

                        if (entryIt == target->arrayValue->end())
                            return false;

                        target = &entryIt->second;
                        bracketPos = closeBracket + 1;

                        while (bracketPos < rawName.size() &&
                            std::isspace(static_cast<unsigned char>(rawName[bracketPos])))
                        {
                            ++bracketPos;
                        }

                        if (bracketPos < rawName.size() &&
                            rawName[bracketPos] != '[')
                        {
                            return false;
                        }
                    }

                    if (target->type == ScriptVariable::INT)
                    {
                        out = target->intValue;
                        return true;
                    }

                    if (target->type == ScriptVariable::FLOAT)
                    {
                        out = target->floatValue;
                        return true;
                    }

                    return false;
                };

            std::function<bool(double&)> parseExpression;
            std::function<bool(double&)> parseTerm;
            std::function<bool(double&)> parseFactor;

            parseFactor = [&](double& out) -> bool
                {
                    skipSpaces();

                    if (pos >= expression.size())
                        return false;

                    if (expression[pos] == '+' || expression[pos] == '-')
                    {
                        char sign = expression[pos++];

                        if (!parseFactor(out))
                            return false;

                        if (sign == '-')
                            out = -out;

                        return true;
                    }

                    if (expression[pos] == '(')
                    {
                        ++pos;

                        if (!parseExpression(out))
                            return false;

                        skipSpaces();

                        if (pos >= expression.size() || expression[pos] != ')')
                            return false;

                        ++pos;
                        return true;
                    }

                    const char* start = expression.c_str() + pos;
                    char* end = nullptr;
                    double number = std::strtod(start, &end);

                    if (end != start)
                    {
                        pos += static_cast<size_t>(end - start);

                        if (pos < expression.size() &&
                            (expression[pos] == 'f' || expression[pos] == 'F'))
                        {
                            ++pos;
                        }

                        out = number;
                        return std::isfinite(out);
                    }

                    // Read a variable name, preserving quoted array keys.
                    size_t startPos = pos;

                    while (pos < expression.size())
                    {
                        char c = expression[pos];

                        if (std::isspace(static_cast<unsigned char>(c)) ||
                            c == '+' || c == '-' || c == '*' || c == '/' ||
                            c == '(' || c == ')')
                        {
                            break;
                        }

                        if (c == '[')
                        {
                            ++pos;

                            if (pos >= expression.size() ||
                                (expression[pos] != '"' && expression[pos] != '\''))
                            {
                                return false;
                            }

                            char quote = expression[pos++];

                            while (pos < expression.size() && expression[pos] != quote)
                                ++pos;

                            if (pos >= expression.size())
                                return false;

                            ++pos;

                            if (pos >= expression.size() || expression[pos] != ']')
                                return false;

                            ++pos;
                            continue;
                        }

                        ++pos;
                    }

                    if (startPos == pos)
                        return false;

                    std::string variableName =
                        expression.substr(startPos, pos - startPos);

                    return getNumericVariable(variableName, out);
                };

            parseTerm = [&](double& out) -> bool
                {
                    if (!parseFactor(out))
                        return false;

                    while (true)
                    {
                        skipSpaces();

                        if (pos >= expression.size() ||
                            (expression[pos] != '*' && expression[pos] != '/'))
                        {
                            break;
                        }

                        char operation = expression[pos++];
                        double rhs = 0.0;

                        if (!parseFactor(rhs))
                            return false;

                        if (operation == '*')
                        {
                            out *= rhs;
                        }
                        else
                        {
                            if (rhs == 0.0)
                                return false;

                            out /= rhs;
                        }

                        if (!std::isfinite(out))
                            return false;
                    }

                    return true;
                };

            parseExpression = [&](double& out) -> bool
                {
                    if (!parseTerm(out))
                        return false;

                    while (true)
                    {
                        skipSpaces();

                        if (pos >= expression.size() ||
                            (expression[pos] != '+' && expression[pos] != '-'))
                        {
                            break;
                        }

                        char operation = expression[pos++];
                        double rhs = 0.0;

                        if (!parseTerm(rhs))
                            return false;

                        if (operation == '+')
                            out += rhs;
                        else
                            out -= rhs;

                        if (!std::isfinite(out))
                            return false;
                    }

                    return true;
                };

            if (!parseExpression(result))
                return false;

            skipSpaces();

            return pos == expression.size() && std::isfinite(result);
        };

        // ========================================================
        // DYNAMIC ARRAY INDEX AND STRING INTERPOLATION RESOLUTION
        // ========================================================
        std::function<std::string(const std::string&)> resolveDynamicIndices;

        resolveDynamicIndices = [&](const std::string& rawPath) -> std::string
            {
                std::string resolved;
                resolved.reserve(rawPath.size());

                for (size_t i = 0; i < rawPath.size();)
                {
                    if (rawPath[i] != '[')
                    {
                        resolved += rawPath[i++];
                        continue;
                    }

                    size_t end = i + 1;
                    int depth = 1;
                    char quote = '\0';

                    for (; end < rawPath.size() && depth > 0; ++end)
                    {
                        char c = rawPath[end];

                        if (quote != '\0')
                        {
                            if (c == quote)
                                quote = '\0';

                            continue;
                        }

                        if (c == '"' || c == '\'')
                        {
                            quote = c;
                            continue;
                        }

                        if (c == '[')
                            ++depth;
                        else if (c == ']')
                            --depth;
                    }

                    if (depth != 0)
                        return {};

                    std::string key = rawPath.substr(i + 1, end - i - 2);
                    trim(key);

                    // Preserve existing quoted string keys.
                    if (key.size() >= 2 &&
                        ((key.front() == '"' && key.back() == '"') ||
                            (key.front() == '\'' && key.back() == '\'')))
                    {
                        resolved += "[" + key + "]";
                        i = end;
                        continue;
                    }

                    // Resolve nested dynamic indices recursively.
                    key = resolveDynamicIndices(key);

                    if (key.empty())
                        return {};

                    double numericIndex = 0.0;

                    if (!evaluateExpression(key, numericIndex) ||
                        numericIndex < 0.0 ||
                        numericIndex > static_cast<double>(
                            (std::numeric_limits<int>::max)()) ||
                        numericIndex != std::floor(numericIndex))
                    {
                        std::cout << "[ScriptReader] Invalid dynamic array index: "
                            << key << std::endl;

                        return {};
                    }

                    resolved += "[\"" +
                        std::to_string(static_cast<int>(numericIndex)) + "\"]";

                    i = end;
                }

                return resolved;
            };

        auto resolveInterpolationValue = [&](const std::string& expression,
            std::string& output) -> bool
            {
                std::string path = expression;
                trim(path);

                path = resolveDynamicIndices(path);
                if (path.empty())
                    return false;

                path = resolveVariableAlias(path);

                // Scalar variable.
                auto scalar = variables.find(path);
                if (scalar != variables.end())
                {
                    const ScriptVariable& variable = scalar->second;

                    switch (variable.type)
                    {
                    case ScriptVariable::STRING:
                        output = variable.stringValue;
                        return true;

                    case ScriptVariable::BOOL:
                        output = variable.boolValue ? "true" : "false";
                        return true;

                    case ScriptVariable::INT:
                        output = std::to_string(variable.intValue);
                        return true;

                    case ScriptVariable::FLOAT:
                        output = std::to_string(variable.floatValue);
                        return true;

                    default:
                        return false;
                    }
                }

                // Nested array entry.
                size_t firstBracket = path.find('[');
                if (firstBracket == std::string::npos)
                    return false;

                std::string rootName = path.substr(0, firstBracket);
                trim(rootName);

                auto rootIt = variables.find(rootName);
                if (rootIt == variables.end())
                    return false;

                ScriptVariable* target = &rootIt->second;
                size_t pos = firstBracket;

                while (pos < path.size())
                {
                    if (path[pos] != '[')
                        return false;

                    size_t closeBracket = path.find(']', pos + 1);
                    if (closeBracket == std::string::npos)
                        return false;

                    std::string key = path.substr(
                        pos + 1,
                        closeBracket - pos - 1);

                    trim(key);

                    if (key.size() < 2 ||
                        !((key.front() == '"' && key.back() == '"') ||
                            (key.front() == '\'' && key.back() == '\'')))
                    {
                        return false;
                    }

                    key = key.substr(1, key.size() - 2);

                    if (target->type != ScriptVariable::ARRAY ||
                        !target->arrayValue)
                    {
                        return false;
                    }

                    auto entryIt = target->arrayValue->find(key);
                    if (entryIt == target->arrayValue->end())
                        return false;

                    target = &entryIt->second;
                    pos = closeBracket + 1;

                    while (pos < path.size() &&
                        std::isspace(static_cast<unsigned char>(path[pos])))
                    {
                        ++pos;
                    }
                }

                switch (target->type)
                {
                case ScriptVariable::STRING:
                    output = target->stringValue;
                    return true;

                case ScriptVariable::BOOL:
                    output = target->boolValue ? "true" : "false";
                    return true;

                case ScriptVariable::INT:
                    output = std::to_string(target->intValue);
                    return true;

                case ScriptVariable::FLOAT:
                    output = std::to_string(target->floatValue);
                    return true;

                default:
                    return false;
                }
            };

        auto resolveInterpolatedString = [&](std::string& input) -> bool
            {
                // Only process the interpolated-string syntax: $"..."
                if (input.size() < 3 ||
                    input[0] != '$' ||
                    input[1] != '"' ||
                    input.back() != '"')
                {
                    return true;
                }

                std::string source = input.substr(2, input.size() - 3);
                std::string output;

                for (size_t i = 0; i < source.size();)
                {
                    if (source[i] != '{')
                    {
                        output += source[i++];
                        continue;
                    }

                    size_t close = source.find('}', i + 1);
                    if (close == std::string::npos)
                        return false;

                    std::string expression = source.substr(
                        i + 1, close - i - 1);

                    std::string replacement;
                    if (!resolveInterpolationValue(expression, replacement))
                    {
                        std::cout << "[ScriptReader] Cannot resolve interpolation: "
                            << expression << std::endl;
                        return false;
                    }

                    output += replacement;
                    i = close + 1;
                }

                // Return a regular quoted string for the existing type detector.
                input = "\"" + output + "\"";
                return true;
            };

        // Resolve dynamic indices before Variable() parses the destination.
        if (arrayEntry)
        {
            name = resolveDynamicIndices(name);
            if (name.empty())
                return false;
        }

        if (!resolveInterpolatedString(value))
            return false;

        // ========================================================
        // ARRAY ENTRY ASSIGNMENT
        // ========================================================
        if (arrayEntry && op == "=")
        {
            // Resolve array references or evaluate arithmetic expressions
            // before determining the destination array entry's type.
            if (value.find('[') != std::string::npos &&
                !(value.size() >= 2 && value.front() == '"') &&
                !(value.size() >= 3 && value[0] == '$' && value[1] == '"'))
            {
                std::string resolvedValue;

                if (resolveInterpolationValue(value, resolvedValue))
                {
                    value = resolvedValue;
                }
                else
                {
                    // If resolving the whole value fails, try evaluating it
                    // as a numeric expression containing array references.
                    float evaluatedValue = 0.0f;

                    if (EvaluateFloatExpression(value, evaluatedValue))
                    {
                        value = std::to_string(evaluatedValue);
                    }
                    else
                    {
                        std::cout
                            << "[ScriptReader] Cannot resolve array assignment value: "
                            << value << std::endl;

                        return false;
                    }
                }
            }

            ScriptVariable::Type type;

            if (value == "undefined")
            {
                type = ScriptVariable::UNDEFINED;
            }
            else if (value == "true" || value == "false")
            {
                type = ScriptVariable::BOOL;
            }
            else if (value.size() >= 2 &&
                value.front() == '"' && value.back() == '"')
            {
                type = ScriptVariable::STRING;
                value = value.substr(1, value.size() - 2);
            }
            else
            {
                std::string numeric = value;

                if (!numeric.empty() &&
                    (numeric.back() == 'f' || numeric.back() == 'F'))
                {
                    numeric.pop_back();
                }

                try
                {
                    size_t parsed = 0;
                    std::stoi(numeric, &parsed);

                    if (parsed == numeric.size())
                    {
                        type = ScriptVariable::INT;
                    }
                    else
                    {
                        throw std::invalid_argument("Not an integer");
                    }
                }
                catch (...)
                {
                    try
                    {
                        size_t parsed = 0;
                        std::stof(numeric, &parsed);

                        if (parsed != numeric.size())
                            return false;

                        type = ScriptVariable::FLOAT;
                        value = numeric;
                    }
                    catch (...)
                    {
                        std::cout
                            << "[ScriptReader] Cannot determine array value type: "
                            << name << " = " << value << std::endl;

                        return false;
                    }
                }
            }

            return Variable("Set", type, name, value, nullptr);
        }

    // ========================================================
    // ARRAY ENTRY ARITHMETIC
    // ========================================================
    if (arrayEntry && arithmetic)
    {
        std::string arrayName;
        std::vector<std::string> keys;
        size_t pos = name.find('[');

        if (pos == std::string::npos)
            return false;

        arrayName = name.substr(0, pos);
        trim(arrayName);

        arrayName = resolveVariableAlias(arrayName);

        while (pos < name.size())
        {
            if (name[pos] != '[')
            {
                std::cout << "[ScriptReader] Invalid array entry: "
                    << name << std::endl;
                return false;
            }

            size_t closeBracket = name.find(']', pos + 1);

            if (closeBracket == std::string::npos)
                return false;

            std::string key = name.substr(pos + 1, closeBracket - pos - 1);
            trim(key);

            if (key.size() < 2 ||
                !((key.front() == '"' && key.back() == '"') ||
                    (key.front() == '\'' && key.back() == '\'')))
            {
                std::cout << "[ScriptReader] Array keys must be quoted: "
                    << name << std::endl;
                return false;
            }

            keys.push_back(key.substr(1, key.size() - 2));
            pos = closeBracket + 1;

            while (pos < name.size() &&
                std::isspace(static_cast<unsigned char>(name[pos])))
            {
                ++pos;
            }
        }

        auto arrayIt = variables.find(arrayName);

        if (arrayIt == variables.end() ||
            arrayIt->second.type != ScriptVariable::ARRAY ||
            !arrayIt->second.arrayValue)
        {
            std::cout << "[ScriptReader] Array not found: "
                << arrayName << std::endl;
            return false;
        }

        ScriptVariable* target = &arrayIt->second;

        for (const auto& key : keys)
        {
            if (target->type != ScriptVariable::ARRAY ||
                !target->arrayValue)
            {
                std::cout << "[ScriptReader] Invalid nested array: "
                    << name << std::endl;
                return false;
            }

            auto entryIt = target->arrayValue->find(key);

            if (entryIt == target->arrayValue->end())
            {
                std::cout << "[ScriptReader] Array entry not found: "
                    << name << std::endl;
                return false;
            }

            target = &entryIt->second;
        }

        if (target->type != ScriptVariable::INT &&
            target->type != ScriptVariable::FLOAT)
        {
            std::cout << "[ScriptReader] Array arithmetic requires an existing numeric entry: "
                << name << std::endl;
            return false;
        }

        double amount = 0.0;

        if (!evaluateExpression(value, amount))
        {
            std::cout << "[ScriptReader] Invalid arithmetic expression: "
                << value << std::endl;
            return false;
        }

        double currentValue = target->type == ScriptVariable::INT
            ? static_cast<double>(target->intValue)
            : static_cast<double>(target->floatValue);

        double result = currentValue;

        if (op == "+=" || op == "++")
            result += amount;
        else if (op == "-=" || op == "--")
            result -= amount;
        else if (op == "*=")
            result *= amount;
        else if (op == "/=")
        {
            if (amount == 0.0)
            {
                std::cout << "[ScriptReader] Division by zero: "
                    << name << std::endl;
                return false;
            }

            result /= amount;
        }
        else
        {
            return false;
        }

        if (!std::isfinite(result))
            return false;

        std::string updatedValue;

        if (target->type == ScriptVariable::INT)
        {
            if (result < (std::numeric_limits<int>::min)() ||
                result >(std::numeric_limits<int>::max)())
            {
                return false;
            }

            target->intValue = static_cast<int>(result);
            updatedValue = std::to_string(target->intValue);
        }
        else
        {
            if (result < -(std::numeric_limits<float>::max)() ||
                result >(std::numeric_limits<float>::max)())
            {
                return false;
            }

            target->floatValue = static_cast<float>(result);
            updatedValue = std::to_string(target->floatValue);
        }

        RefreshUIElements();

        std::cout << "[ScriptReader] Array entry updated: "
            << name << " " << op << " -> " << updatedValue << std::endl;

        return true;
    }

    // ========================================================
    // SCALAR VARIABLE ARITHMETIC
    // ========================================================
    if (arithmetic)
    {
        auto it = variables.find(name);

        if (it == variables.end())
        {
            std::cout << "[ScriptReader] Cannot use " << op
                << " on undefined variable: " << name << std::endl;
            return false;
        }

        if (it->second.type != ScriptVariable::INT &&
            it->second.type != ScriptVariable::FLOAT)
        {
            std::cout << "[ScriptReader] Arithmetic requires a numeric variable: "
                << name << std::endl;
            return false;
        }

        double amount = 0.0;

        if (!evaluateExpression(value, amount))
        {
            std::cout << "[ScriptReader] Invalid arithmetic expression: "
                << value << std::endl;
            return false;
        }

        double currentValue = it->second.type == ScriptVariable::INT
            ? static_cast<double>(it->second.intValue)
            : static_cast<double>(it->second.floatValue);

        double result = currentValue;

        if (op == "++" || op == "+=")
            result += amount;
        else if (op == "--" || op == "-=")
            result -= amount;
        else if (op == "*=")
            result *= amount;
        else if (op == "/=")
        {
            if (amount == 0.0)
            {
                std::cout << "[ScriptReader] Division by zero: "
                    << name << std::endl;
                return false;
            }

            result /= amount;
        }
        else
        {
            return false;
        }

        if (!std::isfinite(result))
            return false;

        std::string updatedValue;

        if (it->second.type == ScriptVariable::INT)
        {
            if (result < (std::numeric_limits<int>::min)() ||
                result >(std::numeric_limits<int>::max)())
            {
                return false;
            }

            updatedValue = std::to_string(static_cast<int>(result));
        }
        else
        {
            if (result < -(std::numeric_limits<float>::max)() ||
                result >(std::numeric_limits<float>::max)())
            {
                return false;
            }

            updatedValue = std::to_string(static_cast<float>(result));
        }

        if (!Variable("Set", it->second.type, name, updatedValue, nullptr))
            return false;

        std::cout << "[ScriptReader] Variable updated: "
            << name << " " << op << " -> " << updatedValue << std::endl;

        return true;
    }

    // ========================================================
    // RESOLVE VARIABLE ASSIGNMENT VALUES
    // Supports scalar and nested array-entry RHS values.
    // ========================================================
    if (op == "=")
    {
        std::string sourceName = resolveVariableAlias(value);
        std::string resolvedValue;

        if (resolveInterpolationValue(sourceName, resolvedValue))
        {
            value = resolvedValue;
        }
        else if (value.find('[') != std::string::npos)
        {
            std::cout << "[ScriptReader] Cannot resolve variable value: "
                << value << std::endl;
            return false;
        }
        else
        {
            auto sourceIt = variables.find(sourceName);

            if (sourceIt != variables.end())
            {
                const ScriptVariable& source = sourceIt->second;

                switch (source.type)
                {
                case ScriptVariable::BOOL:
                    value = source.boolValue ? "true" : "false";
                    break;

                case ScriptVariable::INT:
                    value = std::to_string(source.intValue);
                    break;

                case ScriptVariable::FLOAT:
                    value = std::to_string(source.floatValue);
                    break;

                case ScriptVariable::STRING:
                    value = "\"" + source.stringValue + "\"";
                    break;

                default:
                    break;
                }
            }
        }
    }

    // ========================================================
    // NORMAL SCALAR VARIABLE ASSIGNMENT
    // ========================================================
    ScriptVariable::Type variableType;

    if (value == "true" || value == "false")
    {
        variableType = ScriptVariable::BOOL;
    }
    else if (value.size() >= 2 &&
        value.front() == '"' && value.back() == '"')
    {
        variableType = ScriptVariable::STRING;
        value = value.substr(1, value.size() - 2);
    }
    else
    {
        std::string numeric = value;

        if (!numeric.empty() &&
            (numeric.back() == 'f' || numeric.back() == 'F'))
        {
            numeric.pop_back();
        }

        try
        {
            size_t parsed = 0;
            std::stoi(numeric, &parsed);

            if (parsed == numeric.size())
                variableType = ScriptVariable::INT;
            else
                throw std::invalid_argument("Not an integer");
        }
        catch (...)
        {
            try
            {
                size_t parsed = 0;
                std::stof(numeric, &parsed);

                if (parsed != numeric.size())
                    return false;

                variableType = ScriptVariable::FLOAT;
                value = numeric;
            }
            catch (...)
            {
                std::cout << "[ScriptReader] Cannot determine variable type: "
                    << name << " = " << value << std::endl;
                return false;
            }
        }
    }

    auto it = variables.find(name);

    if (it == variables.end())
    {
        ScriptVariable newVariable{};
        newVariable.type = variableType;
        variables[name] = newVariable;
    }
    else if (it->second.type != variableType)
    {
        std::cout << "[ScriptReader] Variable type mismatch: "
            << name << std::endl;
        return false;
    }

    if (!Variable("Set", variableType, name, value, nullptr))
        return false;

    std::cout << "[ScriptReader] Variable set: "
        << name << " = " << value << std::endl;

    return true;
}

bool ScriptReader::ExecuteIf(
    const std::string& condition,
    const std::string& trueBody,
    const std::string& falseBody,
    bool* chained,
    unsigned long long threadId,
    size_t commandIndex)
{
    // ========================================================
    // EVALUATE CONDITION
    // ========================================================

    bool result = false;

    if (!EvaluateCondition(condition, result))
    {
        std::cout
            << "[ScriptReader] Failed to evaluate if condition: "
            << condition
            << std::endl;

        return false;
    }

    // ========================================================
    // SELECT BODY
    // ========================================================

    const std::string& body = result ? trueBody : falseBody;

    if (body.empty())
        return true;

    // ========================================================
    // PARSE BODY
    // ========================================================

    std::vector<ScriptCommand> commands;

    if (!ParseCommands(body, commands))
    {
        std::cout
            << "[ScriptReader] Failed to parse if body."
            << std::endl;

        return false;
    }

    // ========================================================
    // SORT COMMANDS
    // ========================================================

    std::stable_sort(
        commands.begin(),
        commands.end(),
        [](const ScriptCommand& a, const ScriptCommand& b)
        {
            return a.order < b.order;
        });

    // ========================================================
    // SAVE BODY FOR RESUMABLE EXECUTION
    // ========================================================

    auto threadIt = std::find_if(
        activeThreads.begin(),
        activeThreads.end(),
        [threadId](const ScriptThread& thread)
        {
            return thread.threadId == threadId;
        });

    if (threadIt == activeThreads.end())
    {
        std::cout
            << "[ScriptReader] Cannot execute if body: thread not found."
            << std::endl;

        return false;
    }

    ScriptExecutionState state;
    state.commands = std::move(commands);
    state.commandIndex = 0;

    threadIt->executionStack.push_back(std::move(state));

    return true;
}
bool conditionAlreadyRan = true;
bool scriptVarPAR = true;

bool ScriptReader::EvaluateCondition(
    const std::string& condition,
    bool& result)
{
    struct ConditionValue
    {
        double value = 0.0;

        bool AsBool() const
        {
            return value != 0.0;
        }
    };

    size_t pos = 0;

    auto SkipWhitespace = [&]()
        {
            while (pos < condition.size() &&
                std::isspace(static_cast<unsigned char>(condition[pos])))
            {
                ++pos;
            }
        };

    // Remove decorative prefixes while preserving nested array keys.
    auto NormalizeVariablePath = [](std::string path) -> std::string
        {
            size_t firstBracket = path.find('[');

            std::string root =
                firstBracket == std::string::npos
                ? path
                : path.substr(0, firstBracket);

            std::string suffix =
                firstBracket == std::string::npos
                ? ""
                : path.substr(firstBracket);

            size_t dot = root.rfind('.');

            if (dot != std::string::npos)
                root = root.substr(dot + 1);

            return root + suffix;
        };

    // Resolve function parameters to their original variable paths.
    auto ResolveVariableAlias = [&](const std::string& rawName) -> std::string
        {
            std::string resolved = NormalizeVariablePath(rawName);

            if (currentThreadIndex < 0 ||
                currentThreadIndex >= static_cast<int>(activeThreads.size()))
            {
                return resolved;
            }

            const auto& aliases =
                activeThreads[currentThreadIndex].parameterAliases;

            // Follow alias chains, including parameters passed through
            // nested user-defined function calls.
            for (size_t depth = 0; depth <= aliases.size(); ++depth)
            {
                size_t bracket = resolved.find('[');

                std::string root =
                    bracket == std::string::npos
                    ? resolved
                    : resolved.substr(0, bracket);

                std::string suffix =
                    bracket == std::string::npos
                    ? ""
                    : resolved.substr(bracket);

                auto aliasIt = aliases.find(root);

                if (aliasIt == aliases.end())
                    break;

                resolved = NormalizeVariablePath(aliasIt->second) + suffix;
            }

            return NormalizeVariablePath(resolved);
        };

    std::function<bool(ConditionValue&)> ParseOr;
    std::function<bool(ConditionValue&)> ParseAnd;
    std::function<bool(ConditionValue&)> ParseComparison;
    std::function<bool(ConditionValue&)> ParseUnary;
    std::function<bool(ConditionValue&)> ParsePrimary;

    // PRIMARY: Variables, literals, parentheses, and button input.
    ParsePrimary = [&](ConditionValue& out) -> bool
        {
            SkipWhitespace();

            if (pos >= condition.size())
                return false;

            if (condition[pos] == '(')
            {
                ++pos;

                if (!ParseOr(out))
                    return false;

                SkipWhitespace();

                if (pos >= condition.size() || condition[pos] != ')')
                    return false;

                ++pos;
                return true;
            }

            if (condition[pos] == '"' || condition[pos] == '\'')
                return false;

            // Parse numeric literals before identifiers.
            {
                const char* begin = condition.c_str() + pos;
                char* end = nullptr;
                const double number = std::strtod(begin, &end);

                if (end != begin)
                {
                    size_t next =
                        static_cast<size_t>(end - condition.c_str());

                    if (next < condition.size() &&
                        (condition[next] == 'f' || condition[next] == 'F'))
                    {
                        ++next;
                    }

                    if (next == condition.size() ||
                        std::isspace(static_cast<unsigned char>(condition[next])) ||
                        condition[next] == ')' ||
                        condition[next] == '<' ||
                        condition[next] == '>' ||
                        condition[next] == '=' ||
                        condition[next] == '!' ||
                        condition.compare(next, 2, "&&") == 0 ||
                        condition.compare(next, 2, "||") == 0)
                    {
                        pos = next;
                        out.value = number;
                        return true;
                    }
                }
            }

            const size_t start = pos;

            // Read a variable name, including nested ["key"] lookups.
            while (pos < condition.size())
            {
                const unsigned char c =
                    static_cast<unsigned char>(condition[pos]);

                if (std::isalnum(c) ||
                    condition[pos] == '_' ||
                    condition[pos] == '.' ||
                    c >= 0x80)
                {
                    ++pos;
                    continue;
                }

                if (condition[pos] == '[')
                {
                    ++pos;
                    SkipWhitespace();

                    if (pos >= condition.size() ||
                        (condition[pos] != '"' && condition[pos] != '\''))
                    {
                        return false;
                    }

                    const char quote = condition[pos++];

                    while (pos < condition.size() &&
                        condition[pos] != quote)
                    {
                        ++pos;
                    }

                    if (pos >= condition.size())
                        return false;

                    ++pos;
                    SkipWhitespace();

                    if (pos >= condition.size() || condition[pos] != ']')
                        return false;

                    ++pos;
                    continue;
                }

                break;
            }

            if (start == pos)
                return false;

            const std::string identifier =
                condition.substr(start, pos - start);

            if (identifier == "true" || identifier == "TRUE")
            {
                out.value = 1.0;
                return true;
            }

            if (identifier == "false" || identifier == "FALSE")
            {
                out.value = 0.0;
                return true;
            }

            // Evaluate ButtonPressed("KEY") and ButtonPressed($"{variable}").
            {
                SkipWhitespace();

                const std::string functionName = "ButtonPressed";

                if (condition.compare(pos, functionName.size(), functionName) == 0)
                {
                    pos += functionName.size();
                    SkipWhitespace();

                    if (pos >= condition.size() || condition[pos] != '(')
                        return false;

                    ++pos;
                    SkipWhitespace();

                    const size_t keyStart = pos;
                    char quote = '\0';
                    int braceDepth = 0;

                    while (pos < condition.size())
                    {
                        const char c = condition[pos];

                        if (quote != '\0')
                        {
                            if (c == quote && braceDepth == 0)
                                quote = '\0';
                            else if (c == '{')
                                ++braceDepth;
                            else if (c == '}' && braceDepth > 0)
                                --braceDepth;
                        }
                        else
                        {
                            if (c == '"' || c == '\'')
                                quote = c;
                            else if (c == ')')
                                break;
                        }

                        ++pos;
                    }

                    if (pos >= condition.size() ||
                        quote != '\0' ||
                        braceDepth != 0)
                    {
                        return false;
                    }

                    std::string key =
                        condition.substr(keyStart, pos - keyStart);

                    ++pos;
                    SkipWhitespace();

                    if (key.empty())
                        return false;

                    out.value = ExecuteButtonPressed({ key }) ? 1.0 : 0.0;
                    return true;
                }
            }

            // Resolve the variable root and all nested array keys.
            std::string variablePath =
                ResolveVariableAlias(identifier);

            std::string variableName = variablePath;
            std::vector<std::string> arrayKeys;

            const size_t bracket = variablePath.find('[');

            if (bracket != std::string::npos)
            {
                variableName = variablePath.substr(0, bracket);

                size_t keyPos = bracket;

                while (keyPos < variablePath.size())
                {
                    if (variablePath[keyPos] != '[')
                        return false;

                    ++keyPos;

                    while (keyPos < variablePath.size() &&
                        std::isspace(
                            static_cast<unsigned char>(variablePath[keyPos])))
                    {
                        ++keyPos;
                    }

                    if (keyPos >= variablePath.size() ||
                        (variablePath[keyPos] != '"' &&
                            variablePath[keyPos] != '\''))
                    {
                        return false;
                    }

                    const char quote = variablePath[keyPos++];
                    const size_t keyStart = keyPos;

                    while (keyPos < variablePath.size() &&
                        variablePath[keyPos] != quote)
                    {
                        ++keyPos;
                    }

                    if (keyPos >= variablePath.size())
                        return false;

                    arrayKeys.push_back(
                        variablePath.substr(keyStart, keyPos - keyStart));

                    ++keyPos;

                    while (keyPos < variablePath.size() &&
                        std::isspace(
                            static_cast<unsigned char>(variablePath[keyPos])))
                    {
                        ++keyPos;
                    }

                    if (keyPos >= variablePath.size() ||
                        variablePath[keyPos] != ']')
                    {
                        return false;
                    }

                    ++keyPos;
                }
            }

            auto it = variables.find(variableName);

            if (it == variables.end())
            {
                std::cout
                    << "[ScriptReader][Condition] Unknown variable: "
                    << variableName
                    << " (expression: " << identifier << ")"
                    << std::endl;

                return false;
            }

            const ScriptVariable* variable = &it->second;

            for (const std::string& arrayKey : arrayKeys)
            {
                if (variable->type != ScriptVariable::ARRAY ||
                    !variable->arrayValue)
                {
                    std::cout
                        << "[ScriptReader][Condition] Not an array at key: "
                        << arrayKey
                        << std::endl;

                    return false;
                }

                auto entry = variable->arrayValue->find(arrayKey);

                if (entry == variable->arrayValue->end() ||
                    entry->second.type == ScriptVariable::UNDEFINED)
                {
                    out.value = 0.0;
                    return true;
                }

                variable = &entry->second;
            }

            switch (variable->type)
            {
            case ScriptVariable::BOOL:
                out.value = variable->boolValue ? 1.0 : 0.0;
                break;

            case ScriptVariable::INT:
                out.value = static_cast<double>(variable->intValue);
                break;

            case ScriptVariable::FLOAT:
                out.value = static_cast<double>(variable->floatValue);
                break;

            default:
                std::cout
                    << "[ScriptReader][Condition] Unsupported variable type: "
                    << identifier
                    << std::endl;

                return false;
            }

            static bool debugConditions = true;

            if (debugConditions)
            {
                std::cout
                    << "[ScriptReader][Condition] Variable: "
                    << identifier
                    << " | Resolved: "
                    << variablePath
                    << " | Value: "
                    << out.value
                    << std::endl;

                debugConditions = false;
            }

            return true;
        };

    // Unary negation: !value
    ParseUnary = [&](ConditionValue& out) -> bool
        {
            SkipWhitespace();

            if (pos < condition.size() &&
                condition[pos] == '!' &&
                (pos + 1 >= condition.size() ||
                    condition[pos + 1] != '='))
            {
                ++pos;

                if (!ParseUnary(out))
                    return false;

                out.value = out.AsBool() ? 0.0 : 1.0;
                return true;
            }

            return ParsePrimary(out);
        };

    // Numeric comparisons: ==, !=, <, >, <=, >=
    ParseComparison = [&](ConditionValue& out) -> bool
        {
            if (!ParseUnary(out))
                return false;

            SkipWhitespace();

            std::string op;

            if (condition.compare(pos, 2, "==") == 0 ||
                condition.compare(pos, 2, "!=") == 0 ||
                condition.compare(pos, 2, "<=") == 0 ||
                condition.compare(pos, 2, ">=") == 0)
            {
                op = condition.substr(pos, 2);
                pos += 2;
            }
            else if (pos < condition.size() &&
                (condition[pos] == '<' || condition[pos] == '>'))
            {
                op = condition.substr(pos, 1);
                ++pos;
            }
            else
            {
                return true;
            }

            ConditionValue right;

            if (!ParseUnary(right))
                return false;

            bool comparison = false;

            if (op == "==")
                comparison = out.value == right.value;
            else if (op == "!=")
                comparison = out.value != right.value;
            else if (op == "<")
                comparison = out.value < right.value;
            else if (op == ">")
                comparison = out.value > right.value;
            else if (op == "<=")
                comparison = out.value <= right.value;
            else if (op == ">=")
                comparison = out.value >= right.value;

            out.value = comparison ? 1.0 : 0.0;
            return true;
        };

    // Logical AND: &&
    ParseAnd = [&](ConditionValue& out) -> bool
        {
            if (!ParseComparison(out))
                return false;

            while (true)
            {
                SkipWhitespace();

                if (condition.compare(pos, 2, "&&") != 0)
                    break;

                pos += 2;

                ConditionValue right;

                if (!ParseComparison(right))
                    return false;

                out.value =
                    (out.AsBool() && right.AsBool()) ? 1.0 : 0.0;
            }

            return true;
        };

    // Logical OR: ||
    ParseOr = [&](ConditionValue& out) -> bool
        {
            if (!ParseAnd(out))
                return false;

            while (true)
            {
                SkipWhitespace();

                if (condition.compare(pos, 2, "||") != 0)
                    break;

                pos += 2;

                ConditionValue right;

                if (!ParseAnd(right))
                    return false;

                out.value =
                    (out.AsBool() || right.AsBool()) ? 1.0 : 0.0;
            }

            return true;
        };

    // Evaluate the complete condition.
    ConditionValue evaluated;

    if (!ParseOr(evaluated))
    {
        std::cout
            << "[ScriptReader][Condition] Failed to evaluate: "
            << condition
            << std::endl;

        return false;
    }

    SkipWhitespace();

    if (pos != condition.size())
    {
        std::cout
            << "[ScriptReader][Condition] Unexpected text: "
            << condition.substr(pos)
            << std::endl;

        return false;
    }

    result = evaluated.AsBool();

    //std::cout
    //    << "[ScriptReader][Condition] Result: "
    //    << (result ? "true" : "false")
    //    << " | Expression: "
    //    << condition
    //    << std::endl;

    return true;
}

static bool SplitFunctionArguments(
    const std::string& text,
    std::vector<std::string>& arguments)
{
    arguments.clear();

    if (text.find_first_not_of(" \t\r\n") == std::string::npos)
        return true;

    size_t start = 0;
    int parentheses = 0;
    int brackets = 0;
    int braces = 0;
    char quote = '\0';
    bool escaped = false;

    for (size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];

        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (quote != '\0')
        {
            if (c == '\\')
            {
                escaped = true;
                continue;
            }

            if (c == quote)
                quote = '\0';

            continue;
        }

        if (c == '"' || c == '\'')
        {
            quote = c;
            continue;
        }

        switch (c)
        {
        case '(':
            ++parentheses;
            break;

        case ')':
            if (--parentheses < 0)
                return false;
            break;

        case '[':
            ++brackets;
            break;

        case ']':
            if (--brackets < 0)
                return false;
            break;

        case '{':
            ++braces;
            break;

        case '}':
            if (--braces < 0)
                return false;
            break;

        case ',':
            if (parentheses == 0 &&
                brackets == 0 &&
                braces == 0)
            {
                std::string argument =
                    text.substr(start, i - start);

                const size_t first =
                    argument.find_first_not_of(" \t\r\n");

                if (first == std::string::npos)
                    return false;

                const size_t last =
                    argument.find_last_not_of(" \t\r\n");

                arguments.push_back(
                    argument.substr(first, last - first + 1));

                start = i + 1;
            }
            break;
        }
    }

    if (quote != '\0' ||
        parentheses != 0 ||
        brackets != 0 ||
        braces != 0)
    {
        return false;
    }

    std::string argument = text.substr(start);

    const size_t first =
        argument.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
        return false;

    const size_t last =
        argument.find_last_not_of(" \t\r\n");

    arguments.push_back(
        argument.substr(first, last - first + 1));

    return true;
}

bool ScriptReader::ExecuteCommand(
    const ScriptCommand& command,
    bool* chained,
    unsigned long long threadId,
    size_t commandIndex)
{
    const std::string& text = command.command;

    // ========================================================
    // NORMALIZE PREFIX
    // Any prefix before the first space is accepted.
    // ========================================================

    std::string normalizedCommand = text;
    size_t prefixEnd = text.find(' ');

    if (prefixEnd != std::string::npos)
    {
        normalizedCommand = text.substr(prefixEnd + 1);

        size_t first = normalizedCommand.find_first_not_of(" \t");

        if (first != std::string::npos)
            normalizedCommand = normalizedCommand.substr(first);
        else
            normalizedCommand.clear();
    }

    // ========================================================
    // WAIT
    // ========================================================

    if (normalizedCommand == "wait" ||
        normalizedCommand.rfind("wait(", 0) == 0)
    {
        std::string argument;

        if (command.args.size() == 1)
        {
            argument = command.args[0];
        }
        else
        {
            size_t waitPos = normalizedCommand.find("wait");
            argument = normalizedCommand.substr(waitPos + 4);

            size_t first = argument.find_first_not_of(" \t(");

            if (first == std::string::npos)
                return false;

            argument = argument.substr(first);

            size_t end = argument.find_first_of("); \t");

            if (end != std::string::npos)
                argument.resize(end);
        }

        size_t first = argument.find_first_not_of(" \t(");

        if (first == std::string::npos)
            return false;

        argument = argument.substr(first);

        size_t last = argument.find_last_not_of(" \t)");

        if (last == std::string::npos)
            return false;

        argument.resize(last + 1);

        double milliseconds = 0.0;

        try
        {
            size_t parsed = 0;
            milliseconds = std::stod(argument, &parsed);

            if (parsed != argument.size() ||
                !std::isfinite(milliseconds) ||
                milliseconds < 0.0)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        auto threadIt = std::find_if(
            activeThreads.begin(),
            activeThreads.end(),
            [threadId](const ScriptThread& thread)
            {
                return thread.threadId == threadId;
            });

        if (threadIt == activeThreads.end())
            return false;

        const auto waitUntil = std::chrono::steady_clock::now() +
            std::chrono::duration_cast<
            std::chrono::steady_clock::duration>(
                std::chrono::duration<double, std::milli>(
                    milliseconds));

        if (!threadIt->executionStack.empty())
        {
            ScriptExecutionState& state =
                threadIt->executionStack.back();

            state.waitUntil = waitUntil;
            state.waiting = milliseconds > 0.0;
        }
        else
        {
            threadIt->waitUntil = waitUntil;
            threadIt->waiting = milliseconds > 0.0;
        }

        return true;
    }

    // ========================================================
    // CREATE RECTANGLE
    // ========================================================

    if (normalizedCommand == "CreateRectangle")
    {
        return ExecuteCreateRectangle(
            command.args,
            threadId,
            commandIndex);
    }

    //=========================================================

    if (normalizedCommand == "ClearUI")
    {
        return ExecuteClearUI(
            command.args,
            threadId,
            commandIndex);
    }

    // ========================================================
    // CREATE TEXT
    // ========================================================

    if (normalizedCommand == "CreateText")
    {
        return ExecuteCreateText(
            command.args,
            threadId,
            commandIndex);
    }

    // ========================================================
    // BUTTON PRESSED
    // ========================================================

    if (normalizedCommand == "ButtonPressed")
        return ExecuteButtonPressed(command.args);

    // ========================================================
    // SET ENTITY STATE
    // ========================================================

    if (normalizedCommand == "SetEntityState")
        return ExecuteSetEntityState(command.args);

    // ========================================================
    // VARIABLE ASSIGNMENT
    // ========================================================

    if (text == "__assign")
    {
        if (command.args.size() != 1)
            return false;

        return ExecuteVariableDeclaration(
            command.args[0],
            threadId,
            commandIndex);
    }

    // ========================================================
    // BACKWARD-COMPATIBLE TYPE-PREFIXED ASSIGNMENTS
    // ========================================================

    const char* typePrefixes[] = {
        "int ",
        "float ",
        "bool ",
        "string "
    };

    for (const char* prefix : typePrefixes)
    {
        const size_t prefixLength = std::strlen(prefix);

        if (text.rfind(prefix, 0) == 0)
            return ExecuteVariableDeclaration(
                text.substr(prefixLength),
                threadId,
                commandIndex);
    }

    // ========================================================
    // DIRECT VARIABLE ASSIGNMENT
    // ========================================================

    if (text.find('=') != std::string::npos ||
        text.find("++") != std::string::npos ||
        text.find("--") != std::string::npos)
    {
        return ExecuteVariableDeclaration(
            text,
            threadId,
            commandIndex);
    }

    // ========================================================
    // FOR LOOP
    // ========================================================

    if (normalizedCommand == "for")
    {
        if (command.args.size() != 3)
            return false;

        std::vector<ScriptCommand> body;
        std::string cleanedBody = RemoveComments(command.trueBody);

        if (!ParseCommands(cleanedBody, body))
            return false;

        std::stable_sort(
            body.begin(),
            body.end(),
            [](const ScriptCommand& a, const ScriptCommand& b)
            {
                return a.sort < b.sort;
            });

        return ExecuteForLoop(
            command.args[0],
            command.args[1],
            command.args[2],
            body,
            threadId,
            commandIndex);
    }

    // ========================================================
    // IF
    // ========================================================

    if (normalizedCommand == "if")
    {
        return ExecuteIf(
            command.condition,
            command.trueBody,
            command.falseBody,
            chained,
            threadId,
            commandIndex);
    }

    // ========================================================
    // FUNCTION CALL / EXPLICIT THREAD
    // ========================================================

    if (prefixEnd != std::string::npos &&
        !normalizedCommand.empty())
    {
        std::string functionCall = normalizedCommand;

        size_t last = functionCall.find_last_not_of(" \t");

        if (last != std::string::npos)
            functionCall.resize(last + 1);

        bool isThread = false;
        const std::string threadKeyword = "thread";

        if (functionCall.compare(
            0,
            threadKeyword.size(),
            threadKeyword) == 0 &&
            (functionCall.size() == threadKeyword.size() ||
                functionCall[threadKeyword.size()] == ' ' ||
                functionCall[threadKeyword.size()] == '\t'))
        {
            isThread = true;
            functionCall = functionCall.substr(threadKeyword.size());

            size_t first = functionCall.find_first_not_of(" \t");

            if (first == std::string::npos)
                return false;

            functionCall = functionCall.substr(first);
        }

        // ----------------------------------------------------
        // EXTRACT FUNCTION NAME AND ARGUMENTS
        // ----------------------------------------------------

        std::string functionName;
        std::vector<std::string> functionArgs;

        size_t openParen = functionCall.find('(');

        if (openParen != std::string::npos)
        {
            size_t closeParen = functionCall.find_last_not_of(" \t");

            if (closeParen == std::string::npos ||
                functionCall[closeParen] != ')')
            {
                return false;
            }

            functionName = functionCall.substr(0, openParen);

            std::string argumentText = functionCall.substr(
                openParen + 1,
                closeParen - openParen - 1);

            if (!SplitFunctionArguments(argumentText, functionArgs))
            {
                std::cout
                    << "[ScriptReader] Invalid function arguments: "
                    << functionName
                    << std::endl;

                return false;
            }
        }
        else
        {
            functionName = functionCall;
            functionArgs = command.args;
        }

        // Trim function name.
        size_t nameFirst = functionName.find_first_not_of(" \t");
        size_t nameLast = functionName.find_last_not_of(" \t");

        if (nameFirst == std::string::npos)
            return false;

        functionName = functionName.substr(
            nameFirst,
            nameLast - nameFirst + 1);

        auto functionIt = functions.find(functionName);

        if (functionIt == functions.end())
            return false;

        // Validate the argument count.
        if (functionArgs.size() !=
            functionIt->second.parameters.size())
        {
            std::cout
                << "[ScriptReader] Argument count mismatch for "
                << functionName
                << " | Expected: "
                << functionIt->second.parameters.size()
                << " | Received: "
                << functionArgs.size()
                << std::endl;

            return false;
        }

        // ----------------------------------------------------
        // EXPLICIT THREAD
        // ----------------------------------------------------

        if (isThread)
        {
            // Normalize a parameter or variable path.
            // Decorative prefixes are ignored, while array keys
            // and nested array paths are preserved.
            auto normalizeReference = [](std::string value) -> std::string
                {
                    auto first = value.find_first_not_of(" \t");
                    if (first == std::string::npos)
                        return {};

                    value = value.substr(first);

                    auto last = value.find_last_not_of(" \t");
                    if (last != std::string::npos)
                        value.resize(last + 1);

                    const size_t bracket = value.find('[');

                    std::string root =
                        bracket == std::string::npos
                        ? value
                        : value.substr(0, bracket);

                    const std::string suffix =
                        bracket == std::string::npos
                        ? ""
                        : value.substr(bracket);

                    auto rootFirst = root.find_first_not_of(" \t");
                    if (rootFirst != std::string::npos)
                        root = root.substr(rootFirst);

                    auto rootLast = root.find_last_not_of(" \t");
                    if (rootLast != std::string::npos)
                        root.resize(rootLast + 1);

                    const size_t dot = root.find_last_of('.');
                    if (dot != std::string::npos)
                        root = root.substr(dot + 1);

                    return root + suffix;
                };

            // Preserve the calling thread's aliases before any
            // existing thread's aliases are modified.
            std::unordered_map<std::string, std::string> parentAliases;

            if (currentThreadIndex < activeThreads.size())
            {
                parentAliases =
                    activeThreads[currentThreadIndex].parameterAliases;
            }

            // Resolve an argument through the calling thread's aliases.
            auto resolveParentAlias =
                [&](std::string argument) -> std::string
                {
                    argument = normalizeReference(argument);

                    for (size_t depth = 0;
                        depth <= parentAliases.size();
                        ++depth)
                    {
                        const size_t bracket = argument.find('[');

                        const std::string root =
                            bracket == std::string::npos
                            ? argument
                            : argument.substr(0, bracket);

                        const std::string suffix =
                            bracket == std::string::npos
                            ? ""
                            : argument.substr(bracket);

                        auto aliasIt = parentAliases.find(root);

                        if (aliasIt == parentAliases.end())
                            break;

                        const std::string target =
                            normalizeReference(aliasIt->second);

                        const std::string next = target + suffix;

                        if (next == argument)
                            break;

                        argument = next;
                    }

                    return normalizeReference(argument);
                };

            auto existingThread = std::find_if(
                activeThreads.begin(),
                activeThreads.end(),
                [&functionName](const ScriptThread& thread)
                {
                    return thread.functionName == functionName;
                });

            if (existingThread != activeThreads.end())
            {
                // Do not restart a thread that is already active.
                if (existingThread->active)
                    return true;

                // Rebuild parameter aliases rather than assigning
                // argument values into new variables.
                std::unordered_map<std::string, std::string> newAliases;

                for (size_t i = 0; i < functionArgs.size(); ++i)
                {
                    const std::string parameter =
                        normalizeReference(
                            functionIt->second.parameters[i]);

                    const std::string argument =
                        resolveParentAlias(functionArgs[i]);

                    if (parameter.empty() || argument.empty())
                    {
                        std::cout
                            << "[ScriptReader] Invalid parameter reference"
                            << " | Function: " << functionName
                            << " | Parameter: "
                            << functionIt->second.parameters[i]
                            << " | Argument: " << functionArgs[i]
                            << std::endl;

                        return false;
                    }

                    newAliases[parameter] = argument;

                    std::cout
                        << "[ScriptReader][Alias] | Function: "
                        << functionName
                        << " | Parameter: " << parameter
                        << " -> " << argument
                        << std::endl;
                }

                std::string cleanedBody =
                    RemoveComments(functionIt->second.body);

                std::vector<ScriptCommand> commands;

                if (!ParseCommands(cleanedBody, commands))
                    return false;

                std::stable_sort(
                    commands.begin(),
                    commands.end(),
                    [](const ScriptCommand& a, const ScriptCommand& b)
                    {
                        return a.sort < b.sort;
                    });

                existingThread->parameterAliases =
                    std::move(newAliases);

                existingThread->commands = std::move(commands);
                existingThread->arguments = functionArgs;
                existingThread->commandIndex = 0;
                existingThread->waitTime = 0.0f;
                existingThread->active = true;
                existingThread->chained = false;
                existingThread->restartRequested = false;
                existingThread->loop = false;
                existingThread->waiting = false;
                existingThread->waitUntil = {};
                existingThread->executionStack.clear();

                return true;
            }

            return StartThread(
                functionName,
                false,
                functionArgs);
        }

        // ----------------------------------------------------
        // NORMAL FUNCTION CALL
        // ----------------------------------------------------

        return Call(
            functionName.c_str(),
            functionArgs);
    }

    return false;
}
bool ScriptReader::ParseCommands(
    const std::string& source,
    std::vector<ScriptCommand>& commands)
{
    commands.clear();

        size_t position = 0;
    size_t commandOrder = 0;

    // ========================================================
    // WHITESPACE
    // ========================================================

    auto SkipWhitespace =
        [&](size_t& pos)
        {
            while (
                pos < source.size() &&
                std::isspace(
                    static_cast<unsigned char>(source[pos])))
            {
                ++pos;
            }
        };

    auto Trim =
        [](std::string value) -> std::string
        {
            size_t start = 0;

            while (
                start < value.size() &&
                std::isspace(
                    static_cast<unsigned char>(value[start])))
            {
                ++start;
            }

            size_t end = value.size();

            while (
                end > start &&
                std::isspace(
                    static_cast<unsigned char>(value[end - 1])))
            {
                --end;
            }

            return value.substr(start, end - start);
        };

    // ========================================================
    // FIND MATCHING PARENTHESES / BRACES
    // ========================================================

    auto FindMatching =
        [&](size_t open, char opening, char closing) -> size_t
        {
            if (
                open >= source.size() ||
                source[open] != opening)
            {
                return std::string::npos;
            }

            int depth = 0;
            char quote = '\0';
            bool escaped = false;

            for (size_t i = open; i < source.size(); ++i)
            {
                const char c = source[i];

                if (quote != '\0')
                {
                    if (escaped)
                    {
                        escaped = false;
                        continue;
                    }

                    if (c == '\\')
                    {
                        escaped = true;
                        continue;
                    }

                    if (c == quote)
                        quote = '\0';

                    continue;
                }

                if (c == '"' || c == '\'')
                {
                    quote = c;
                    continue;
                }

                if (c == opening)
                {
                    ++depth;
                }
                else if (c == closing)
                {
                    --depth;

                    if (depth == 0)
                        return i;
                }
            }

            return std::string::npos;
        };

    // ========================================================
    // ADD COMMAND
    // ========================================================

    auto AddCommand =
        [&](const std::string& name,
            const std::vector<std::string>& args,
            int sort)
        {
            ScriptCommand parsed;

            parsed.command = name;
            parsed.args = args;
            parsed.sort = sort;
            parsed.order = commandOrder++;

            commands.push_back(std::move(parsed));

            const auto& parsedIf = commands.back();

            //std::cout
            //    << "[ScriptReader][Parse] Command #" << parsedIf.order
            //    << " | Name: " << parsedIf.command
            //    << " | Args: " << parsedIf.args.size()
            //    << std::endl;
        };

    // ========================================================
    // PARSE ONE CONDITIONAL CHAIN
    // ========================================================

    std::function<bool(size_t&, ScriptCommand&)> ParseConditional;

    ParseConditional =
        [&](size_t& pos, ScriptCommand& output) -> bool
        {
            const size_t ifStart = pos;

            size_t openParen = ifStart + 2;

            SkipWhitespace(openParen);

            if (
                openParen >= source.size() ||
                source[openParen] != '(')
            {
                //std::cout
                //    << "[ScriptReader] Expected '(' after if."
                //    << std::endl;

                return false;
            }

            size_t closeParen =
                FindMatching(openParen, '(', ')');

            if (closeParen == std::string::npos)
            {
                //std::cout
                //    << "[ScriptReader] Unclosed if condition."
                //    << std::endl;

                return false;
            }

            std::string condition = Trim(
                source.substr(
                    openParen + 1,
                    closeParen - openParen - 1));

            size_t bodyStart = closeParen + 1;

            SkipWhitespace(bodyStart);

            if (
                bodyStart >= source.size() ||
                source[bodyStart] != '{')
            {
                std::cout
                    << "[ScriptReader] Missing '{' for if body."
                    << std::endl;

                return false;
            }

            size_t bodyEnd =
                FindMatching(bodyStart, '{', '}');

            if (bodyEnd == std::string::npos)
            {
                std::cout
                    << "[ScriptReader] Unclosed if body."
                    << std::endl;

                return false;
            }

            std::string trueBody =
                source.substr(
                    bodyStart + 1,
                    bodyEnd - bodyStart - 1);

            std::string falseBody;

            size_t next = bodyEnd + 1;

            SkipWhitespace(next);

            // ------------------------------------------------
            // ELSE / ELSE IF
            // ------------------------------------------------

            const bool hasElse =
                source.compare(next, 4, "else") == 0 &&
                (
                    next + 4 >= source.size() ||
                    std::isspace(
                        static_cast<unsigned char>(
                            source[next + 4])) ||
                    source[next + 4] == '{'
                    );

            if (hasElse)
            {
                size_t afterElse = next + 4;

                SkipWhitespace(afterElse);

                const bool hasElseIf =
                    source.compare(afterElse, 2, "if") == 0 &&
                    (
                        afterElse + 2 >= source.size() ||
                        std::isspace(
                            static_cast<unsigned char>(
                                source[afterElse + 2])) ||
                        source[afterElse + 2] == '('
                        );

                if (hasElseIf)
                {
                    ScriptCommand nestedIf;

                    size_t nestedPosition = afterElse;

                    if (
                        !ParseConditional(
                            nestedPosition,
                            nestedIf))
                    {
                        return false;
                    }

                    falseBody =
                        "if (" +
                        nestedIf.condition +
                        ") {" +
                        nestedIf.trueBody +
                        "}";

                    if (!nestedIf.falseBody.empty())
                    {
                        falseBody +=
                            " else {" +
                            nestedIf.falseBody +
                            "}";
                    }

                    next = nestedPosition;
                }
                else
                {
                    if (
                        afterElse >= source.size() ||
                        source[afterElse] != '{')
                    {
                        std::cout
                            << "[ScriptReader] Missing '{' for else body."
                            << std::endl;

                        return false;
                    }

                    size_t elseEnd =
                        FindMatching(afterElse, '{', '}');

                    if (elseEnd == std::string::npos)
                    {
                        std::cout
                            << "[ScriptReader] Unclosed else body."
                            << std::endl;

                        return false;
                    }

                    falseBody =
                        source.substr(
                            afterElse + 1,
                            elseEnd - afterElse - 1);

                    next = elseEnd + 1;
                }
            }

            output = ScriptCommand{};

            output.command = "if";
            output.condition = condition;
            output.trueBody = trueBody;
            output.falseBody = falseBody;

            output.args =
            {
                condition,
                trueBody,
                falseBody
            };

            output.sort = 0;
            output.order = commandOrder++;

            pos = next;

            return true;
        };


        // ========================================================
        // PARSE FOR LOOP
        // ========================================================

        std::function<bool(size_t&, ScriptCommand&)> ParseForLoop;

        ParseForLoop =
            [&](size_t& pos, ScriptCommand& output) -> bool
            {
                size_t openParen = pos + 3;
                SkipWhitespace(openParen);

                if (openParen >= source.size() || source[openParen] != '(')
                {
                    std::cout << "[ScriptReader] Expected '(' after for." << std::endl;
                    return false;
                }

                size_t closeParen = FindMatching(openParen, '(', ')');

                if (closeParen == std::string::npos)
                {
                    std::cout << "[ScriptReader] Unclosed for header." << std::endl;
                    return false;
                }

                std::string header = source.substr(
                    openParen + 1,
                    closeParen - openParen - 1);

                // Split the header at top-level semicolons.
                std::vector<std::string> args;
                std::string current;
                char quote = '\0';
                bool escaped = false;
                int parenDepth = 0;

                for (char c : header)
                {
                    if (quote != '\0')
                    {
                        current += c;

                        if (escaped)
                        {
                            escaped = false;
                            continue;
                        }

                        if (c == '\\')
                        {
                            escaped = true;
                            continue;
                        }

                        if (c == quote)
                            quote = '\0';

                        continue;
                    }

                    if (c == '"' || c == '\'')
                    {
                        quote = c;
                        current += c;
                        continue;
                    }

                    if (c == '(')
                        ++parenDepth;
                    else if (c == ')' && parenDepth > 0)
                        --parenDepth;

                    if (c == ';' && parenDepth == 0)
                    {
                        args.push_back(Trim(current));
                        current.clear();
                        continue;
                    }

                    current += c;
                }

                args.push_back(Trim(current));

                if (args.size() != 3)
                {
                    std::cout
                        << "[ScriptReader] For loop requires three header sections."
                        << std::endl;
                    return false;
                }

                size_t bodyStart = closeParen + 1;
                SkipWhitespace(bodyStart);

                if (bodyStart >= source.size() || source[bodyStart] != '{')
                {
                    std::cout << "[ScriptReader] Missing '{' for for loop body." << std::endl;
                    return false;
                }

                size_t bodyEnd = FindMatching(bodyStart, '{', '}');

                if (bodyEnd == std::string::npos)
                {
                    std::cout << "[ScriptReader] Unclosed for loop body." << std::endl;
                    return false;
                }

                output = ScriptCommand{};
                output.command = "for";
                output.args = std::move(args);
                output.trueBody = source.substr(
                    bodyStart + 1,
                    bodyEnd - bodyStart - 1);
                output.sort = 0;
                output.order = commandOrder++;

                pos = bodyEnd + 1;

                std::cout
                    << "[ScriptReader][Parse] For loop #"
                    << output.order
                    << " | Initializer: [" << output.args[0] << "]"
                    << " | Condition: [" << output.args[1] << "]"
                    << " | Increment: [" << output.args[2] << "]"
                    << " | Body: " << output.trueBody.size() << " chars"
                    << std::endl;

                return true;
            };

    // ========================================================
    // MAIN PARSER
    // ========================================================

    while (position < source.size())
    {
        SkipWhitespace(position);

        if (position >= source.size())
            break;

        // ----------------------------------------------------
        // FOR LOOP
        // ----------------------------------------------------

        if (
            source.compare(position, 3, "for") == 0 &&
            (
                position + 3 >= source.size() ||
                std::isspace(static_cast<unsigned char>(source[position + 3])) ||
                source[position + 3] == '('
                ))
        {
            ScriptCommand parsed;

            if (!ParseForLoop(position, parsed))
                return false;

            commands.push_back(std::move(parsed));
            continue;
        }

        // ----------------------------------------------------
        // IF / ELSE IF / ELSE
        // ----------------------------------------------------

        if (
            source.compare(position, 2, "if") == 0 &&
            (
                position + 2 >= source.size() ||
                std::isspace(
                    static_cast<unsigned char>(
                        source[position + 2])) ||
                source[position + 2] == '('
                ))
        {
            ScriptCommand parsed;

            if (!ParseConditional(position, parsed))
                return false;

            commands.push_back(std::move(parsed));

            const auto& parsedIf = commands.back();

            //std::cout
            //    << "[ScriptReader][Parse] Conditional #"
            //    << parsedIf.order
            //    << " | Condition: [" << parsedIf.condition << "]"
            //    << " | True body: " << parsedIf.trueBody.size() << " chars"
            //    << " | False body: " << parsedIf.falseBody.size() << " chars"
            //    << std::endl;

            continue;
        }

        // ----------------------------------------------------
        // FIND STATEMENT BOUNDARY
        //
        // A semicolon outside quotes and parentheses/braces
        // marks the end of a declaration or assignment.
        // ----------------------------------------------------

        size_t statementEnd = std::string::npos;
        size_t parenDepth = 0;
        size_t braceDepth = 0;
        char statementQuote = '\0';
        bool statementEscaped = false;

        for (size_t i = position; i < source.size(); ++i)
        {
            const char c = source[i];

            if (statementQuote != '\0')
            {
                if (statementEscaped)
                {
                    statementEscaped = false;
                    continue;
                }

                if (c == '\\')
                {
                    statementEscaped = true;
                    continue;
                }

                if (c == statementQuote)
                    statementQuote = '\0';

                continue;
            }

            if (c == '"' || c == '\'')
            {
                statementQuote = c;
                continue;
            }

            if (c == '(')
                ++parenDepth;
            else if (c == ')' && parenDepth > 0)
                --parenDepth;
            else if (c == '{')
                ++braceDepth;
            else if (c == '}' && braceDepth > 0)
                --braceDepth;

            if (
                c == ';' &&
                parenDepth == 0 &&
                braceDepth == 0)
            {
                statementEnd = i;
                break;
            }

            // Don't scan through a later independent block
            // while trying to identify a simple statement.
            if (
                c == '{' &&
                parenDepth == 0 &&
                braceDepth == 1)
            {
                break;
            }
        }

        // ----------------------------------------------------
        // WAIT COMMAND
        // ----------------------------------------------------

        if (statementEnd != std::string::npos)
        {
            std::string statement = Trim(source.substr(position, statementEnd - position));
            std::string waitStatement = statement;

            size_t prefixEnd = waitStatement.find_first_of(" \t");
            if (prefixEnd != std::string::npos)
                waitStatement = Trim(waitStatement.substr(prefixEnd + 1));

            if (waitStatement.compare(0, 4, "wait") == 0 &&
                (waitStatement.size() == 4 ||
                    std::isspace(static_cast<unsigned char>(waitStatement[4])) ||
                    waitStatement[4] == '('))
            {
                std::string duration = Trim(waitStatement.substr(4));

                if (!duration.empty() && duration.front() == '(' && duration.back() == ')')
                    duration = Trim(duration.substr(1, duration.size() - 2));

                if (duration.empty())
                {
                    std::cout << "[ScriptReader] Missing wait duration." << std::endl;
                    return false;
                }

                AddCommand("wait", { duration }, 0);
                position = statementEnd + 1;
                continue;
            }

            if (!statement.empty())
            {
                const bool isDeclaration =
                    statement.rfind("int ", 0) == 0 ||
                    statement.rfind("float ", 0) == 0 ||
                    statement.rfind("bool ", 0) == 0 ||
                    statement.rfind("string ", 0) == 0;

                // ----------------------------------------------------
                // VARIABLE DECLARATION
                // ----------------------------------------------------

                if (isDeclaration)
                {
                    AddCommand(statement, {}, 0);

                    position = statementEnd + 1;
                    continue;
                }


                // ----------------------------------------------------
                // FIND ASSIGNMENT OPERATOR
                // ----------------------------------------------------

                size_t assignmentPos = std::string::npos;
                size_t operatorLength = 1;

                char quote = '\0';
                bool escaped = false;

                for (size_t i = 0; i < statement.size(); ++i)
                {
                    const char c = statement[i];

                    if (quote != '\0')
                    {
                        if (escaped)
                        {
                            escaped = false;
                            continue;
                        }

                        if (c == '\\')
                        {
                            escaped = true;
                            continue;
                        }

                        if (c == quote)
                            quote = '\0';

                        continue;
                    }

                    if (c == '"' || c == '\'')
                    {
                        quote = c;
                        continue;
                    }

                    // Handle ++ and -- before looking for '='.
                    if (i + 1 < statement.size() &&
                        ((c == '+' && statement[i + 1] == '+') ||
                            (c == '-' && statement[i + 1] == '-')))
                    {
                        assignmentPos = i;
                        operatorLength = 2;
                        break;
                    }

                    if (c != '=')
                        continue;

                    const char previous =
                        i > 0 ? statement[i - 1] : '\0';

                    const char next =
                        i + 1 < statement.size()
                        ? statement[i + 1]
                        : '\0';

                    if (previous == '=' || previous == '!' ||
                        previous == '<' || previous == '>' ||
                        next == '=')
                    {
                        continue;
                    }

                    // Keep += and -= attached to the assignment operator.
                    if (previous == '+' || previous == '-')
                    {
                        assignmentPos = i - 1;
                        operatorLength = 2;
                    }
                    else
                    {
                        assignmentPos = i;
                        operatorLength = 1;
                    }

                    break;
                }

                // ----------------------------------------------------
                // VARIABLE ASSIGNMENT
                //
                // Only consume the statement if it looks like an
                // assignment. Function calls fall through below.
                // ----------------------------------------------------


                if (assignmentPos != std::string::npos)
                {
                    const std::string variableName =
                        Trim(statement.substr(0, assignmentPos));

                    const std::string value =
                        Trim(statement.substr(assignmentPos + operatorLength));

                    const std::string assignmentOperator =
                        statement.substr(assignmentPos, operatorLength);

                    if (variableName.empty() ||
                        (value.empty() &&
                            assignmentOperator != "++" &&
                            assignmentOperator != "--"))
                    {
                        std::cout
                            << "[ScriptReader] Invalid assignment: "
                            << statement
                            << std::endl;

                        return false;
                    }

                    const std::string assignment =
                        variableName + " " + assignmentOperator + " " + value;

                    AddCommand(
                        "__assign",
                        {
                            statement
                        },
                        0);

                    std::cout
                        << "[ScriptReader][AssignParse]"
                        << " Statement=[" << statement << "]"
                        << " Name=[" << variableName << "]"
                        << " Operator=[" << assignmentOperator << "]"
                        << " Value=[" << value << "]"
                        << std::endl;

                    position = statementEnd + 1;
                    continue;
                }
            }
        }

        // If this wasn't a declaration or assignment, do NOT advance
        // position here. Let the ordinary command parser handle it.

        // ----------------------------------------------------
        // ORDINARY FUNCTION-STYLE COMMAND
        // ----------------------------------------------------

        size_t openParen =
            source.find('(', position);

        if (openParen == std::string::npos)
        {
            // Ignore trailing whitespace or standalone braces.
            break;
        }

        std::string command =
            Trim(
                source.substr(
                    position,
                    openParen - position));

        if (command.empty())
        {
            std::cout
                << "[ScriptReader] Empty command name."
                << std::endl;

            return false;
        }

        size_t closeParen =
            FindMatching(openParen, '(', ')');

        if (closeParen == std::string::npos)
        {
            std::cout
                << "[ScriptReader] Missing ')' in: "
                << command
                << std::endl;

            return false;
        }

        std::string arguments =
            source.substr(
                openParen + 1,
                closeParen - openParen - 1);


        // ----------------------------------------------------
        // SPLIT ARGUMENTS
        // ----------------------------------------------------

        std::vector<std::string> args;
        std::string current;

        char quote = '\0';
        bool escaped = false;
        bool interpolatedString = false;
        bool insideInterpolation = false;

        int nestedParenDepth = 0;
        int nestedBraceDepth = 0;
        int interpolationBraceDepth = 0;

        for (size_t i = 0; i < arguments.size(); ++i)
        {
            const char c = arguments[i];

            // Start an interpolated string: $"..."
            if (
                quote == '\0' &&
                c == '$' &&
                i + 1 < arguments.size() &&
                arguments[i + 1] == '"')
            {
                quote = '"';
                interpolatedString = true;

                current += "$\"";
                ++i;
                continue;
            }

            if (quote != '\0')
            {
                current += c;

                if (escaped)
                {
                    escaped = false;
                    continue;
                }

                if (c == '\\')
                {
                    escaped = true;
                    continue;
                }

                // Enter the expression inside an interpolated string.
                if (
                    interpolatedString &&
                    !insideInterpolation &&
                    quote == '"' &&
                    c == '{')
                {
                    quote = '\0';
                    insideInterpolation = true;
                    interpolationBraceDepth = 1;
                    ++nestedBraceDepth;
                    continue;
                }

                if (c == quote)
                {
                    quote = '\0';

                    if (interpolatedString && !insideInterpolation)
                        interpolatedString = false;
                }

                continue;
            }

            // Inside { ... } of an interpolated string.
            // Quoted array keys such as ["MenuHuds"] are allowed here.
            if (insideInterpolation)
            {
                if (c == '{')
                {
                    ++interpolationBraceDepth;
                    ++nestedBraceDepth;
                }
                else if (c == '}')
                {
                    if (interpolationBraceDepth > 0)
                        --interpolationBraceDepth;

                    if (nestedBraceDepth > 0)
                        --nestedBraceDepth;

                    if (interpolationBraceDepth == 0)
                    {
                        insideInterpolation = false;
                        quote = '"';
                    }
                }
                else if (c == '"' || c == '\'')
                {
                    quote = c;
                    current += c;
                    continue;
                }
            }
            else
            {
                if (c == '"' || c == '\'')
                {
                    quote = c;
                    current += c;
                    continue;
                }

                if (c == '(')
                    ++nestedParenDepth;
                else if (c == ')' && nestedParenDepth > 0)
                    --nestedParenDepth;
                else if (c == '{')
                    ++nestedBraceDepth;
                else if (c == '}' && nestedBraceDepth > 0)
                    --nestedBraceDepth;
            }

            if (
                c == ',' &&
                nestedParenDepth == 0 &&
                nestedBraceDepth == 0)
            {
                args.push_back(Trim(current));
                current.clear();
                continue;
            }

            current += c;
        }

        if (!current.empty() || !arguments.empty())
            args.push_back(Trim(current));

        // ----------------------------------------------------
        // SORT VALUE
        // ----------------------------------------------------

        int sort = 0;
        
        size_t prefixEnd = command.find_last_of(' ');
        std::string commandName = prefixEnd == std::string::npos ? command : command.substr(prefixEnd + 1);
        
        if (commandName == "CreateRectangle" || commandName == "CreateText")
        {
            if (args.size() != 7)
            {
                std::cout << "[ScriptReader] " << command << " expects 7 arguments, received " << args.size() << std::endl;
                return false;
            }
        
            try
            {
                sort = std::stoi(args[6]);
            }
            catch (...)
            {
                std::cout << "[ScriptReader] Invalid sort value: " << args[6] << std::endl;
                return false;
            }
        }
        
        AddCommand(command, args, sort);

        // ----------------------------------------------------
        // ADVANCE PAST THIS COMMAND
        // ----------------------------------------------------

        position = closeParen + 1;

        SkipWhitespace(position);

        if (
            position < source.size() &&
            source[position] == ';')
        {
            ++position;
        }
    }
    return true;
}

static bool ContainsButtonPressed(const std::string& condition) 
{ 
    return condition.find("ButtonPressed(") != std::string::npos; 
}

void ScriptReader::UpdateThreads()
{
    if (activeThreads.empty())
    {
        currentThreadIndex = -1;
        return;
    }

    size_t i = 0;

    while (i < activeThreads.size())
    {
        if (!activeThreads[i].active)
        {
            ++i;
            continue;
        }

        if (activeThreads[i].waiting)
        {
            if (std::chrono::steady_clock::now() < activeThreads[i].waitUntil)
            {
                ++i;
                continue;
            }

            activeThreads[i].waiting = false;
        }

        currentThreadIndex = static_cast<int>(i);

        const unsigned long long threadId = activeThreads[i].threadId;
        bool chained = activeThreads[i].chained;

        bool hasButtonConditions = false;

        for (const auto& cmd : activeThreads[i].commands)
        {
            if (cmd.command == "if" && ContainsButtonPressed(cmd.condition))
            {
                hasButtonConditions = true;
                break;
            }
        }

        size_t commandsChecked = 0;
        const size_t maxCommands = activeThreads[i].commands.size() + 10000;

        while (i < activeThreads.size() &&
            activeThreads[i].active &&
            commandsChecked < maxCommands)
        {
            if (activeThreads[i].waiting)
                break;

            bool nestedWaiting = false;

            if (!activeThreads[i].executionStack.empty())
            {
                ScriptExecutionState& state =
                    activeThreads[i].executionStack.back();

                if (state.waiting)
                {
                    if (std::chrono::steady_clock::now() >= state.waitUntil)
                    {
                        state.waiting = false;
                    }
                    else
                    {
                        nestedWaiting = true;
                    }
                }
            }

            // Preserve existing wait behavior.
            if (nestedWaiting && !hasButtonConditions)
                break;

            const bool executingNested =
                !activeThreads[i].executionStack.empty() &&
                !nestedWaiting;

            ScriptCommand command;
            size_t commandIndex = 0;

            if (executingNested)
            {
                ScriptExecutionState& state =
                    activeThreads[i].executionStack.back();

                if (state.commandIndex >= state.commands.size())
                {
                    if (!state.isForLoop)
                    {
                        activeThreads[i].executionStack.pop_back();
                        continue;
                    }

                    if (state.iterations >= 10000)
                    {
                        activeThreads[i].active = false;
                        break;
                    }

                    const std::string increment = state.increment;

                    if (!increment.empty() &&
                        !ExecuteVariableDeclaration(
                            increment,
                            activeThreads[i].threadId,
                            activeThreads[i].commandIndex))
                    {
                        activeThreads[i].active = false;
                        break;
                    }

                    if (activeThreads[i].executionStack.empty())
                        break;

                    ScriptExecutionState& loopState =
                        activeThreads[i].executionStack.back();

                    loopState.commandIndex = 0;
                    loopState.checkForCondition = true;
                    ++loopState.iterations;

                    continue;
                }

                // Evaluate the complete loop condition, including
                // variable lookups on both sides of the comparison.
                if (state.isForLoop && state.checkForCondition)
                {
                    state.checkForCondition = false;

                    if (!state.condition.empty())
                    {
                        bool keepGoing = false;

                        if (!EvaluateCondition(state.condition, keepGoing))
                        {
                            std::cout
                                << "[ScriptReader][ForLoop] Failed to evaluate condition: "
                                << state.condition
                                << std::endl;

                            activeThreads[i].active = false;
                            break;
                        }

                        if (!keepGoing)
                        {
                            activeThreads[i].executionStack.pop_back();
                            continue;
                        }
                    }
                }

                // Reacquire the state after condition processing.
                if (activeThreads[i].executionStack.empty())
                    continue;

                const size_t stateIndex =
                    activeThreads[i].executionStack.size() - 1;

                if (activeThreads[i].executionStack[stateIndex].commandIndex >=
                    activeThreads[i].executionStack[stateIndex].commands.size())
                {
                    continue;
                }

                commandIndex =
                    activeThreads[i].executionStack[stateIndex].commandIndex;

                command =
                    activeThreads[i].executionStack[stateIndex].commands[commandIndex];

                ++activeThreads[i].executionStack[stateIndex].commandIndex;
            }
            else
            {
                if (nestedWaiting && !hasButtonConditions)
                    break;

                if (activeThreads[i].commandIndex >= activeThreads[i].commands.size())
                {
                    if (activeThreads[i].loop || hasButtonConditions)
                    {
                        activeThreads[i].commandIndex = 0;
                        activeThreads[i].chained = false;
                    }
                    else
                    {
                        activeThreads[i].active = false;
                    }

                    break;
                }

                commandIndex = activeThreads[i].commandIndex;
                command = activeThreads[i].commands[commandIndex];

                ++activeThreads[i].commandIndex;
            }

            ++commandsChecked;
            currentThreadIndex = static_cast<int>(i);

            if (executingNested)
            {
                //std::cout
                //    << "[FOR DEBUG] Executing nested command: "
                //    << command.command
                //    << " | Index: " << commandIndex
                //    << " | Thread: " << threadId
                //    << std::endl;
            }

            const bool result =
                ExecuteCommand(command, &chained, threadId, commandIndex);

            if (command.command.size() >= 11 &&
                command.command.compare(
                    command.command.size() - 10,
                    10,
                    "CreateText") == 0 &&
                std::isspace(static_cast<unsigned char>(
                    command.command[command.command.size() - 11])))
            {
                auto debugThread = std::find_if(
                    activeThreads.begin(),
                    activeThreads.end(),
                    [threadId](const ScriptThread& thread)
                    {
                        return thread.threadId == threadId;
                    });

                if (debugThread != activeThreads.end())
                {
                    std::cout
                        << "[FOR AFTER EXECUTE]"
                        << " | Result: " << result
                        << " | Restart: " << debugThread->restartRequested
                        << " | StackSize: " << debugThread->executionStack.size();

                    if (!debugThread->executionStack.empty())
                    {
                        const auto& state = debugThread->executionStack.back();

                        std::cout
                            << " | CommandIndex: " << state.commandIndex
                            << " | BodySize: " << state.commands.size()
                            << " | IsForLoop: " << state.isForLoop
                            << " | Iterations: " << state.iterations;
                    }

                    std::cout << std::endl;
                }
            }

            // ExecuteCommand may add threads and reallocate activeThreads.
            auto threadIt = std::find_if(
                activeThreads.begin(),
                activeThreads.end(),
                [threadId](const ScriptThread& thread)
                {
                    return thread.threadId == threadId;
                });

            if (threadIt == activeThreads.end())
                break;

            i = static_cast<size_t>(
                std::distance(activeThreads.begin(), threadIt));

            if (!result)
            {
                threadIt->active = false;
                break;
            }

            if (threadIt->restartRequested)
            {
                threadIt->restartRequested = false;
                threadIt->commandIndex = 0;
                threadIt->chained = false;
                threadIt->executionStack.clear();

                std::cout
                    << "[ScriptReader][Restart] "
                    << threadIt->functionName
                    << " restarted after Boolean assignment."
                    << std::endl;

                break;
            }

            threadIt->chained = chained;

            if (threadIt->waiting)
                break;

            if (command.command == "if" &&
                ContainsButtonPressed(command.condition))
            {
                continue;
            }

            // Execute one ordinary command per update.
            break;
        }

        if (i >= activeThreads.size())
            break;

        ++i;
    }

    currentThreadIndex = -1;
}

bool alreadyRanBoolCondition = false;

bool ScriptReader::StartThread(
    const std::string& functionName,
    bool loop,
    const std::vector<std::string>& arguments)
{
    static unsigned long long startCount = 0;
    static unsigned long long nextThreadId = 1;

    ++startCount;

    std::cout
        << "[ScriptReader] StartThread #" << startCount
        << " | Function: " << functionName
        << " | Loop: " << (loop ? "true" : "false")
        << " | Active before: " << activeThreads.size()
        << std::endl;

    // Find the function.
    auto it = functions.find(functionName);

    if (it == functions.end())
    {
        std::cout
            << "[ScriptReader] Cannot start thread. "
            << "Function not found: " << functionName
            << std::endl;

        return false;
    }

    const auto& function = it->second;

    // Validate the number of supplied arguments.
    if (arguments.size() != function.parameters.size())
    {
        std::cout
            << "[ScriptReader] Argument count mismatch for "
            << functionName
            << " | Expected: " << function.parameters.size()
            << " | Received: " << arguments.size()
            << std::endl;

        return false;
    }

    // Copy the executing parent's aliases before adding a new thread.
    std::unordered_map<std::string, std::string> parentAliases;

    if (currentThreadIndex >= 0 &&
        currentThreadIndex < static_cast<int>(activeThreads.size()))
    {
        parentAliases =
            activeThreads[currentThreadIndex].parameterAliases;
    }

    auto trim = [](std::string& value)
        {
            const size_t first = value.find_first_not_of(" \t\r\n");

            if (first == std::string::npos)
            {
                value.clear();
                return;
            }

            const size_t last = value.find_last_not_of(" \t\r\n");

            value = value.substr(first, last - first + 1);
        };

    // Normalize a variable path:
    // - Remove decorative prefixes from the root only.
    // - Preserve every chained associative-array key.
    auto normalizeReference = [&](std::string value) -> std::string
        {
            trim(value);

            const size_t bracket = value.find('[');

            std::string root =
                bracket == std::string::npos
                ? value
                : value.substr(0, bracket);

            const std::string suffix =
                bracket == std::string::npos
                ? ""
                : value.substr(bracket);

            trim(root);

            const size_t dot = root.find_last_of('.');

            if (dot != std::string::npos)
            {
                root = root.substr(dot + 1);
            }

            trim(root);

            return root + suffix;
        };

    // Resolve an argument through the parent's aliases.
    // Only the root variable is aliased; array keys remain unchanged.
    auto resolveParentAlias = [&](std::string argument) -> std::string
        {
            argument = normalizeReference(argument);

            for (size_t depth = 0; depth <= parentAliases.size(); ++depth)
            {
                const size_t bracket = argument.find('[');

                const std::string root =
                    bracket == std::string::npos
                    ? argument
                    : argument.substr(0, bracket);

                const std::string suffix =
                    bracket == std::string::npos
                    ? ""
                    : argument.substr(bracket);

                auto aliasIt = parentAliases.find(root);

                if (aliasIt == parentAliases.end())
                {
                    break;
                }

                // An alias target can itself contain a complete array path.
                const std::string resolvedTarget =
                    normalizeReference(aliasIt->second);

                const std::string nextArgument =
                    resolvedTarget + suffix;

                // Avoid looping on a self-reference.
                if (nextArgument == argument)
                {
                    break;
                }

                argument = nextArgument;
            }

            return normalizeReference(argument);
        };

    // Clean the function body.
    std::string cleanedBody = RemoveComments(function.body);

    // Parse commands.
    std::vector<ScriptCommand> commands;

    if (!ParseCommands(cleanedBody, commands))
    {
        std::cout
            << "[ScriptReader] Failed to parse thread function: "
            << functionName
            << std::endl;

        return false;
    }

    // Sort commands while preserving equal-sort order.
    std::stable_sort(
        commands.begin(),
        commands.end(),
        [](const ScriptCommand& a, const ScriptCommand& b)
        {
            return a.sort < b.sort;
        });

    // Create and initialize the thread.
    ScriptThread thread{};

    thread.threadId = nextThreadId++;
    thread.functionName = functionName;
    thread.arguments = arguments;
    thread.commands = std::move(commands);
    thread.commandIndex = 0;
    thread.waitTime = 0.0f;
    thread.waitUntil = {};
    thread.waiting = false;
    thread.active = true;
    thread.chained = false;
    thread.restartRequested = false;
    thread.loop = loop;

    // Bind parameters to the original variable references.
    for (size_t i = 0; i < arguments.size(); ++i)
    {
        std::string parameter =
            normalizeReference(function.parameters[i]);

        std::string argument =
            resolveParentAlias(arguments[i]);

        if (parameter.empty() || argument.empty())
        {
            std::cout
                << "[ScriptReader] Invalid parameter reference"
                << " | Function: " << functionName
                << " | Parameter: " << parameter
                << " | Argument: " << arguments[i]
                << std::endl;

            return false;
        }

        thread.parameterAliases[parameter] = argument;

        std::cout
            << "[ScriptReader][Alias]"
            << " | Function: " << functionName
            << " | Parameter: " << parameter
            << " -> " << argument
            << std::endl;
    }

    // Add the new thread.
    const unsigned long long startedThreadId = thread.threadId;

    activeThreads.push_back(std::move(thread));

    if (alreadyRanBoolCondition)
    {
        std::cout
            << "[ScriptReader] Started script thread: "
            << functionName
            << " | Thread ID: " << startedThreadId
            << " | Loop: " << (loop ? "true" : "false")
            << " | Active after: " << activeThreads.size()
            << std::endl;

        alreadyRanBoolCondition = false;
    }

    //ClearUI();

    return true;
}

bool ScriptReader::Call(
    const char* functionName,
    const std::vector<std::string>& arguments)
{
    if (!functionName)
    {
        std::cout
            << "[ScriptReader] Call received null function name"
            << std::endl;

        return false;
    }

    auto it = functions.find(functionName);

    if (it == functions.end())
    {
        std::cout
            << "[ScriptReader] Function not found: "
            << functionName
            << std::endl;

        return false;
    }

    if (std::string(functionName).find("init") != std::string::npos)
    {
        std::cout
            << "[ScriptReader] Starting script init()"
            << std::endl;
    }
    else
    {
        std::cout
            << "[ScriptReader] Starting function: "
            << it->second.name
            << std::endl;
    }

    return StartThread(functionName, false, arguments);
}

bool ScriptReader::IsArithmeticExpression(const std::string& expression)
{
    return expression.find_first_of("+-*/()") != std::string::npos;
}

bool ScriptReader::EvaluateFloatExpression(const std::string& expression, float& result)
{
    size_t pos = 0;

    auto skipSpaces = [&]()
        {
            while (pos < expression.size() &&
                std::isspace(static_cast<unsigned char>(expression[pos])))
                ++pos;
        };


    auto getVariableValue = [&](const std::string& variableName, float& value) -> bool
        {
            //std::cout << "[FloatExpression] getVariableValue ENTERED | Input: ["
            //    << variableName << "]" << std::endl;

            std::string name = variableName;
            std::vector<std::string> arrayKeys;

            size_t firstBracket = name.find('[');
            std::string fullRootName = firstBracket == std::string::npos
                ? name
                : name.substr(0, firstBracket);

            auto trim = [](std::string& text)
                {
                    size_t start = text.find_first_not_of(" \t\r\n");
                    if (start == std::string::npos)
                    {
                        text.clear();
                        return;
                    }

                    size_t end = text.find_last_not_of(" \t\r\n");
                    text = text.substr(start, end - start + 1);
                };

            trim(fullRootName);

            std::string rootName = fullRootName;
            size_t dot = rootName.rfind('.');
            if (dot != std::string::npos)
                rootName = rootName.substr(dot + 1);

            //std::cout << "[FloatExpression] Root name: [" << fullRootName
            //    << "] | Fallback: [" << rootName << "]" << std::endl;

            size_t bracketPos = firstBracket;

            while (bracketPos != std::string::npos && bracketPos < name.size())
            {
                if (name[bracketPos] != '[')
                {
                    //std::cout << "[FloatExpression] FAILED: Expected '[' at position "
                    //    << bracketPos << std::endl;
                    return false;
                }

                ++bracketPos;

                while (bracketPos < name.size() &&
                    std::isspace(static_cast<unsigned char>(name[bracketPos])))
                    ++bracketPos;

                if (bracketPos >= name.size() ||
                    (name[bracketPos] != '"' && name[bracketPos] != '\''))
                {
                    //std::cout << "[FloatExpression] FAILED: Expected quoted array key"
                    //    << std::endl;
                    return false;
                }

                char quote = name[bracketPos++];
                size_t keyStart = bracketPos;

                while (bracketPos < name.size() && name[bracketPos] != quote)
                    ++bracketPos;

                if (bracketPos >= name.size())
                {
                    //std::cout << "[FloatExpression] FAILED: Unterminated array key"
                    //    << std::endl;
                    return false;
                }

                arrayKeys.push_back(name.substr(keyStart, bracketPos - keyStart));
                ++bracketPos;

                while (bracketPos < name.size() &&
                    std::isspace(static_cast<unsigned char>(name[bracketPos])))
                    ++bracketPos;

                if (bracketPos >= name.size() || name[bracketPos] != ']')
                {
                    //std::cout << "[FloatExpression] FAILED: Expected ']' after quoted array key"
                    //    << std::endl;
                    return false;
                }

                ++bracketPos;

                while (bracketPos < name.size() &&
                    std::isspace(static_cast<unsigned char>(name[bracketPos])))
                    ++bracketPos;

                if (bracketPos >= name.size())
                    break;

                if (name[bracketPos] != '[')
                {
                    //std::cout << "[FloatExpression] FAILED: Expected next '[' but found '"
                    //    << name[bracketPos] << "'" << std::endl;
                    return false;
                }
            }

            //std::cout << "[FloatExpression] Parsed " << arrayKeys.size()
            //    << " array keys" << std::endl;

            auto rootIt = variables.find(fullRootName);

            if (rootIt == variables.end())
            {
                //std::cout << "[FloatExpression] Full root not found: ["
                //    << fullRootName << "], trying fallback [" << rootName << "]"
                //    << std::endl;

                rootIt = variables.find(rootName);
            }

            if (rootIt == variables.end())
            {
                //std::cout << "[FloatExpression] FAILED: Root not found. Full root: ["
                //    << fullRootName << "] | Fallback: [" << rootName << "]"
                //    << std::endl;
                return false;
            }

            const ScriptVariable* variable = &rootIt->second;

            //std::cout << "[FloatExpression] Root found: [" << rootIt->first
            //    << "] | Type: " << static_cast<int>(variable->type)
            //    << " | Array keys requested: " << arrayKeys.size() << std::endl;

            for (const std::string& key : arrayKeys)
            {
                //std::cout << "[FloatExpression] Resolving key: [" << key << "]";

                if (variable->type != ScriptVariable::ARRAY || !variable->arrayValue)
                {
                    //std::cout << " | FAILED: Parent is not a valid array. Type: "
                    //    << static_cast<int>(variable->type)
                    //    << " | Has array storage: " << (variable->arrayValue != nullptr)
                    //    << std::endl;
                    return false;
                }

                //std::cout << " | Available keys: " << variable->arrayValue->size()
                //    << std::endl;

                auto entry = variable->arrayValue->find(key);

                if (entry == variable->arrayValue->end())
                {
                    //std::cout << "[FloatExpression] FAILED: Key not found: ["
                    //    << key << "]. Existing keys:" << std::endl;

                    for (const auto& item : *variable->arrayValue)
                        //std::cout << "  [" << item.first << "] | Type: "
                        //<< static_cast<int>(item.second.type) << std::endl;

                    return false;
                }

                variable = &entry->second;

                //std::cout << "[FloatExpression] Key resolved: [" << key
                //    << "] | Type: " << static_cast<int>(variable->type)
                //    << " | Has array storage: " << (variable->arrayValue != nullptr)
                //    << std::endl;
            }

            switch (variable->type)
            {
            case ScriptVariable::INT:
                value = static_cast<float>(variable->intValue);
                //std::cout << "[FloatExpression] Numeric INT value: " << value << std::endl;
                return true;

            case ScriptVariable::FLOAT:
                value = variable->floatValue;
                //std::cout << "[FloatExpression] Numeric FLOAT value: " << value << std::endl;
                return std::isfinite(value);

            case ScriptVariable::BOOL:
                value = variable->boolValue ? 1.0f : 0.0f;
                //std::cout << "[FloatExpression] Numeric BOOL value: " << value << std::endl;
                return true;

            case ScriptVariable::STRING:
                try
                {
                    size_t parsed = 0;
                    value = std::stof(variable->stringValue, &parsed);

                    bool valid = parsed == variable->stringValue.size() &&
                        std::isfinite(value);

                    //std::cout << "[FloatExpression] STRING numeric conversion: "
                    //    << (valid ? "SUCCESS" : "FAILED")
                    //    << " | Value: " << variable->stringValue << std::endl;

                    return valid;
                }
                catch (const std::exception& e)
                {
                    //std::cout << "[FloatExpression] STRING conversion exception: "
                    //    << e.what() << std::endl;
                    return false;
                }

            default:
                //std::cout << "[FloatExpression] FAILED: Variable is not numeric. Type: "
                //    << static_cast<int>(variable->type) << std::endl;
                return false;
            }
        };

    std::function<bool(float&)> parseExpression;
    std::function<bool(float&)> parseTerm;
    std::function<bool(float&)> parseFactor;

    parseFactor = [&](float& value) -> bool
        {
            skipSpaces();

            if (pos >= expression.size())
                return false;

            if (expression[pos] == '+' || expression[pos] == '-')
            {
                char op = expression[pos++];

                if (!parseFactor(value))
                    return false;

                if (op == '-')
                    value = -value;

                return std::isfinite(value);
            }

            if (expression[pos] == '(')
            {
                ++pos;

                if (!parseExpression(value))
                    return false;

                skipSpaces();

                if (pos >= expression.size() || expression[pos] != ')')
                    return false;

                ++pos;
                return true;
            }

            if (std::isdigit(static_cast<unsigned char>(expression[pos])) ||
                expression[pos] == '.')
            {
                char* end = nullptr;
                const char* start = expression.c_str() + pos;
                value = std::strtof(start, &end);

                if (end == start || !std::isfinite(value))
                    return false;

                pos += static_cast<size_t>(end - start);

                if (pos < expression.size() &&
                    (expression[pos] == 'f' || expression[pos] == 'F'))
                    ++pos;

                return true;
            }

            size_t start = pos;
            int bracketDepth = 0;
            char quote = '\0';

            while (pos < expression.size())
            {
                char c = expression[pos];

                if (quote != '\0')
                {
                    if (c == quote)
                        quote = '\0';

                    ++pos;
                    continue;
                }

                if (bracketDepth > 0 && (c == '"' || c == '\''))
                {
                    quote = c;
                    ++pos;
                    continue;
                }

                if (c == '[')
                {
                    ++bracketDepth;
                    ++pos;
                    continue;
                }

                if (c == ']')
                {
                    if (bracketDepth == 0)
                        return false;

                    --bracketDepth;
                    ++pos;
                    continue;
                }

                if (bracketDepth == 0 &&
                    (std::isspace(static_cast<unsigned char>(c)) ||
                        c == '+' || c == '-' || c == '*' || c == '/' ||
                        c == '(' || c == ')'))
                    break;

                ++pos;
            }

            if (start == pos || bracketDepth != 0 || quote != '\0')
                return false;

            std::string variableName = expression.substr(start, pos - start);

            //std::cout << "[FloatExpression] Resolving: ["
            //    << variableName << "]" << std::endl;

            bool resolved = getVariableValue(variableName, value);

            //std::cout << "[FloatExpression] Result: "
            //    << (resolved ? std::to_string(value) : "FAILED") << std::endl;

            return resolved;
        };

    parseTerm = [&](float& value) -> bool
        {
            if (!parseFactor(value))
                return false;

            while (true)
            {
                skipSpaces();

                if (pos >= expression.size() ||
                    (expression[pos] != '*' && expression[pos] != '/'))
                    return std::isfinite(value);

                char op = expression[pos++];

                float rhs = 0.0f;
                if (!parseFactor(rhs))
                    return false;

                if (op == '*')
                    value *= rhs;
                else
                {
                    if (rhs == 0.0f)
                        return false;

                    value /= rhs;
                }

                if (!std::isfinite(value))
                    return false;
            }
        };

    parseExpression = [&](float& value) -> bool
        {
            if (!parseTerm(value))
                return false;

            while (true)
            {
                skipSpaces();

                if (pos >= expression.size() ||
                    (expression[pos] != '+' && expression[pos] != '-'))
                    return std::isfinite(value);

                char op = expression[pos++];

                float rhs = 0.0f;
                if (!parseTerm(rhs))
                    return false;

                if (op == '+')
                    value += rhs;
                else
                    value -= rhs;

                if (!std::isfinite(value))
                    return false;
            }
        };

    if (!parseExpression(result))
        return false;

    skipSpaces();
    return pos == expression.size() && std::isfinite(result);
}

bool ScriptReader::ResolveTextExpression(
    const std::string& expression,
    std::string& result) const
{
    auto trim = [](std::string& text)
        {
            size_t start = text.find_first_not_of(" \t\r\n");

            if (start == std::string::npos)
            {
                text.clear();
                return;
            }

            size_t end = text.find_last_not_of(" \t\r\n");
            text = text.substr(start, end - start + 1);
        };

    std::function<bool(const std::string&, std::string&)> getVariableString;

    getVariableString =
        [&](const std::string& variableName, std::string& value) -> bool
        {
            std::string name = variableName;
            trim(name);

            if (name.empty())
                return false;

            size_t bracket = name.find('[');

            std::string rootName =
                bracket == std::string::npos
                ? name
                : name.substr(0, bracket);

            size_t dot = rootName.rfind('.');

            if (dot != std::string::npos)
                rootName = rootName.substr(dot + 1);

            trim(rootName);

            if (rootName.empty())
                return false;

            auto it = variables.find(rootName);

            if (it == variables.end())
                return false;

            const ScriptVariable* variable = &it->second;

            // Resolve each array key, including dynamically evaluated indices.
            while (bracket != std::string::npos)
            {
                // Move inside '['.
                ++bracket;

                while (
                    bracket < name.size() &&
                    std::isspace(static_cast<unsigned char>(name[bracket])))
                {
                    ++bracket;
                }

                size_t keyStart = bracket;
                int nestedBracketDepth = 0;
                char quote = '\0';
                bool escaped = false;

                // Find the matching ']' while respecting nested expressions
                // and quoted strings.
                for (; bracket < name.size(); ++bracket)
                {
                    char c = name[bracket];

                    if (quote != '\0')
                    {
                        if (escaped)
                        {
                            escaped = false;
                            continue;
                        }

                        if (c == '\\')
                        {
                            escaped = true;
                            continue;
                        }

                        if (c == quote)
                            quote = '\0';

                        continue;
                    }

                    if (c == '"' || c == '\'')
                    {
                        quote = c;
                        continue;
                    }

                    if (c == '[')
                    {
                        ++nestedBracketDepth;
                        continue;
                    }

                    if (c == ']')
                    {
                        if (nestedBracketDepth == 0)
                            break;

                        --nestedBracketDepth;
                    }
                }

                if (bracket >= name.size() || quote != '\0')
                    return false;

                std::string keyExpression =
                    name.substr(keyStart, bracket - keyStart);

                trim(keyExpression);

                if (keyExpression.empty())
                    return false;

                std::string key;

                // Literal string key: ["MenuHuds"] or ['MenuHuds'].
                if (
                    keyExpression.size() >= 2 &&
                    (keyExpression.front() == '"' ||
                        keyExpression.front() == '\'') &&
                    keyExpression.back() == keyExpression.front())
                {
                    key = keyExpression.substr(
                        1, keyExpression.size() - 2);
                }
                else
                {
                    // Dynamic key: [i], [menuIndex], or another array lookup.
                    if (!getVariableString(keyExpression, key))
                        return false;
                }

                if (
                    variable->type != ScriptVariable::ARRAY ||
                    !variable->arrayValue)
                {
                    return false;
                }

                auto entry = variable->arrayValue->find(key);

                if (entry == variable->arrayValue->end())
                    return false;

                variable = &entry->second;

                // Move past ']'.
                ++bracket;

                while (
                    bracket < name.size() &&
                    std::isspace(static_cast<unsigned char>(name[bracket])))
                {
                    ++bracket;
                }

                if (bracket >= name.size())
                    break;

                if (name[bracket] != '[')
                    return false;
            }

            // Convert the final resolved value to text.
            switch (variable->type)
            {
            case ScriptVariable::STRING:
                value = variable->stringValue;
                return true;

            case ScriptVariable::BOOL:
                value = variable->boolValue ? "true" : "false";
                return true;

            case ScriptVariable::INT:
                value = std::to_string(variable->intValue);
                return true;

            case ScriptVariable::FLOAT:
                value = std::to_string(variable->floatValue);
                return true;

            default:
                return false;
            }
        };

    // Evaluate a single text value: quoted string, variable,
    // nested array lookup, or interpolated string.
    auto evaluateValue =
        [&](std::string part, std::string& value) -> bool
        {
            trim(part);

            if (part.empty())
            {
                value.clear();
                return true;
            }

            // Interpolated string: $"text {variable}"
            if (
                part.size() >= 3 &&
                part[0] == '$' &&
                part[1] == '"' &&
                part.back() == '"')
            {
                std::string text =
                    part.substr(2, part.size() - 3);

                std::string resolved;
                size_t pos = 0;

                while (pos < text.size())
                {
                    if (text[pos] == '{')
                    {
                        size_t end = text.find('}', pos + 1);

                        if (end == std::string::npos)
                            return false;

                        std::string variableValue;

                        if (!getVariableString(
                            text.substr(pos + 1, end - pos - 1),
                            variableValue))
                        {
                            return false;
                        }

                        resolved += variableValue;
                        pos = end + 1;
                    }
                    else
                    {
                        resolved += text[pos++];
                    }
                }

                value = std::move(resolved);
                return true;
            }

            // Ordinary quoted string.
            if (
                part.size() >= 2 &&
                part.front() == '"' &&
                part.back() == '"')
            {
                value = part.substr(1, part.size() - 2);
                return true;
            }

            // Direct variable or nested array lookup.
            return getVariableString(part, value);
        };

    // Split concatenation on '+' outside quoted strings and array keys.
    std::vector<std::string> parts;
    std::string current;

    char quote = '\0';
    bool escaped = false;
    int bracketDepth = 0;
    bool hasConcatenation = false;

    for (size_t i = 0; i < expression.size(); ++i)
    {
        char c = expression[i];

        if (quote != '\0')
        {
            current += c;

            if (escaped)
            {
                escaped = false;
                continue;
            }

            if (c == '\\')
            {
                escaped = true;
                continue;
            }

            if (c == quote)
                quote = '\0';

            continue;
        }

        // The quote in $"..." starts the outer interpolated string.
        if (
            c == '$' &&
            i + 1 < expression.size() &&
            expression[i + 1] == '"')
        {
            current += c;
            current += expression[++i];
            quote = '"';
            continue;
        }

        if (c == '"' || c == '\'')
        {
            quote = c;
            current += c;
            continue;
        }

        if (c == '[')
            ++bracketDepth;
        else if (c == ']' && bracketDepth > 0)
            --bracketDepth;

        if (c == '+' && bracketDepth == 0)
        {
            parts.push_back(current);
            current.clear();
            hasConcatenation = true;
            continue;
        }

        current += c;
    }

    if (hasConcatenation)
    {
        parts.push_back(current);

        std::string resolved;

        for (const std::string& part : parts)
        {
            std::string value;

            if (!evaluateValue(part, value))
                return false;

            resolved += value;
        }

        result = std::move(resolved);
        return true;
    }

    if (evaluateValue(expression, result))
        return true;

    // Preserve existing behavior for literal, unquoted text.
    result = expression;
    return true;
}
void ScriptReader::RefreshUIElements()
{
    for (auto& element : uiElements)
    {
        if (element.type == ScriptUIElement::RECTANGLE)
        {
            std::string updatedImage;

            if (ResolveTextExpression(element.textExpression, updatedImage))
                element.content = std::move(updatedImage);

            EvaluateFloatExpression(element.xExpression, element.x);
            EvaluateFloatExpression(element.yExpression, element.y);
            EvaluateFloatExpression(element.widthExpression, element.width);
            EvaluateFloatExpression(element.heightExpression, element.height);
            EvaluateFloatExpression(element.alphaExpression, element.alpha);

            try
            {
                element.sort = std::stoi(element.sortExpression);
            }
            catch (const std::exception&)
            {
            }
        }
        else if (element.type == ScriptUIElement::TEXT)
        {
            std::string updatedText;

            if (ResolveTextExpression(element.textExpression, updatedText))
                element.content = std::move(updatedText);

            EvaluateFloatExpression(element.xExpression, element.x);
            EvaluateFloatExpression(element.yExpression, element.y);
            EvaluateFloatExpression(element.widthExpression, element.fontSize);
            EvaluateFloatExpression(element.alphaExpression, element.alpha);

            try
            {
                element.sort = std::stoi(element.sortExpression);
            }
            catch (const std::exception&)
            {
            }
        }
    }
}

std::string ScriptReader::GetUIElementId(
    unsigned long long threadId,
    size_t commandIndex) const
{
    auto it = std::find_if(
        activeThreads.begin(),
        activeThreads.end(),
        [threadId](const ScriptThread& thread)
        {
            return thread.threadId == threadId;
        });

    if (it == activeThreads.end())
        return std::to_string(threadId) + ":" +
        std::to_string(commandIndex);

    return it->functionName + ":" +
        std::to_string(commandIndex);
}

static std::string CaptureForLoopVariable(
    const std::string& expression,
    const ScriptExecutionState& state,
    ScriptReader* reader)
{
    if (!state.isForLoop || state.initializer.empty() || !reader)
        return expression;

    // Extract the complete loop-variable expression before the assignment.
    std::string initializer = state.initializer;
    size_t equals = initializer.find('=');

    if (equals == std::string::npos)
        return expression;

    std::string variableName = initializer.substr(0, equals);

    // Trim whitespace.
    size_t first = variableName.find_first_not_of(" \t\r\n");
    size_t last = variableName.find_last_not_of(" \t\r\n");

    if (first == std::string::npos)
        return expression;

    variableName = variableName.substr(first, last - first + 1);

    // Remove an optional type declaration, such as "int i".
    size_t space = variableName.find_last_of(" \t");

    if (space != std::string::npos)
        variableName = variableName.substr(space + 1);

    if (variableName.empty())
        return expression;

    // Read the current loop value using the complete variable expression.
    float evaluatedValue = 0.0f;

    if (!reader->EvaluateFloatExpression(variableName, evaluatedValue))
        return expression;

    int currentValue = static_cast<int>(evaluatedValue);

    std::string result = expression;
    const std::string replacement = std::to_string(currentValue);

    // Replace only complete occurrences of the loop variable.
    for (size_t pos = 0; pos < result.size();)
    {
        if (result.compare(pos, variableName.size(), variableName) == 0)
        {
            bool leftBoundary = pos == 0 ||
                !(std::isalnum(static_cast<unsigned char>(result[pos - 1])) ||
                    result[pos - 1] == '_');

            size_t end = pos + variableName.size();

            bool rightBoundary = end >= result.size() ||
                !(std::isalnum(static_cast<unsigned char>(result[end])) ||
                    result[end] == '_');

            if (leftBoundary && rightBoundary)
            {
                result.replace(pos, variableName.size(), replacement);
                pos += replacement.size();
                continue;
            }
        }

        ++pos;
    }

    return result;
}

static std::string EscapeTextExpression(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size());

    for (char c : text)
    {
        if (c == '\\' || c == '"')
            escaped += '\\';

        escaped += c;
    }

    return escaped;
}

static std::string CaptureDynamicArrayIndices(
    const std::string& expression,
    ScriptReader* reader)
{
    if (!reader)
        return expression;

    std::string result = expression;

    for (size_t pos = 0; pos < result.size();)
    {
        if (result[pos] != '[')
        {
            ++pos;
            continue;
        }

        // Find the matching closing bracket, respecting nested arrays
        // and quoted string keys.
        size_t end = pos + 1;
        int depth = 1;
        char quote = '\0';
        bool escaped = false;

        for (; end < result.size(); ++end)
        {
            char c = result[end];

            if (quote != '\0')
            {
                if (escaped)
                {
                    escaped = false;
                    continue;
                }

                if (c == '\\')
                {
                    escaped = true;
                    continue;
                }

                if (c == quote)
                    quote = '\0';

                continue;
            }

            if (c == '"' || c == '\'')
            {
                quote = c;
                continue;
            }

            if (c == '[')
                ++depth;
            else if (c == ']' && --depth == 0)
                break;
        }

        if (end >= result.size() || depth != 0)
        {
            ++pos;
            continue;
        }

        std::string keyExpression =
            result.substr(pos + 1, end - pos - 1);

        size_t first = keyExpression.find_first_not_of(" \t\r\n");
        size_t last = keyExpression.find_last_not_of(" \t\r\n");

        if (first == std::string::npos)
        {
            pos = end + 1;
            continue;
        }

        keyExpression =
            keyExpression.substr(first, last - first + 1);

        // Quoted keys such as ["MenuHuds"] are literal strings.
        bool quotedKey =
            keyExpression.size() >= 2 &&
            (keyExpression.front() == '"' ||
                keyExpression.front() == '\'') &&
            keyExpression.back() == keyExpression.front();

        if (quotedKey)
        {
            pos = end + 1;
            continue;
        }

        // Resolve dynamic indices such as [i] or
        // [sora.KingdomHeartVars["MenuHuds"]["i"]].
        float indexValue = 0.0f;

        if (reader->EvaluateFloatExpression(keyExpression, indexValue))
        {
            std::string capturedIndex =
                std::to_string(static_cast<int>(indexValue));

            result.replace(
                pos + 1,
                end - pos - 1,
                capturedIndex);

            pos += capturedIndex.size() + 2;
        }
        else
        {
            pos = end + 1;
        }
    }

    return result;
}

bool ScriptReader::ExecuteCreateText(
    const std::vector<std::string>& args,
    unsigned long long threadId,
    size_t commandIndex)
{
    if (args.size() != 7 && args.size() != 8)
        return false;

    std::string textExpression = args[0];
    std::string text;

    std::cout << "[CreateText] Raw argument: [" << args[0] << "]" << std::endl;
    std::cout << "[CreateText] Arg count: " << args.size() << std::endl;

    if (!ResolveTextExpression(textExpression, text))
        return false;

    std::cout
        << "[CreateText] Resolved text: [" << text << "]"
        << " | Expression: [" << textExpression << "]"
        << std::endl;

    std::string callbackName = args[1];

    float x = 0.0f;
    float y = 0.0f;
    float fontSize = 0.0f;
    float alpha = 0.0f;

    if (!EvaluateFloatExpression(args[2], x) ||
        !EvaluateFloatExpression(args[3], y) ||
        !EvaluateFloatExpression(args[4], fontSize) ||
        !EvaluateFloatExpression(args[5], alpha))
    {
        return false;
    }

    int sort = 0;

    try
    {
        size_t parsed = 0;
        sort = std::stoi(args[6], &parsed);

        if (parsed != args[6].size())
            throw std::invalid_argument("Invalid sort value");
    }
    catch (const std::exception&)
    {
        return false;
    }

    if (functions.find(callbackName) == functions.end())
        return false;

    ScriptUIElement element{};

    element.type = ScriptUIElement::TEXT;
    element.content = text;
    element.textExpression = textExpression;
    element.callbackName = callbackName;
    element.x = x;
    element.y = y;
    element.fontSize = fontSize;
    element.alpha = alpha;
    element.sort = sort;

    element.xExpression = args[2];
    element.yExpression = args[3];
    element.widthExpression = args[4];
    element.alphaExpression = args[5];
    element.sortExpression = args[6];
    element.id = GetUIElementId(threadId, commandIndex);

    auto threadIt = std::find_if(
        activeThreads.begin(),
        activeThreads.end(),
        [threadId](const ScriptThread& thread)
        {
            return thread.threadId == threadId;
        });

    if (threadIt != activeThreads.end() &&
        !threadIt->executionStack.empty())
    {
        const auto& state = threadIt->executionStack.back();

        element.xExpression =
            CaptureForLoopVariable(args[2], state, this);

        element.yExpression =
            CaptureForLoopVariable(args[3], state, this);

        element.widthExpression =
            CaptureForLoopVariable(args[4], state, this);

        element.alphaExpression =
            CaptureForLoopVariable(args[5], state, this);

        if (state.isForLoop)
        {
            element.id += ":for:" + std::to_string(state.iterations);

            // Preserve this iteration's resolved text.
            element.textExpression =
                "\"" + EscapeTextExpression(text) + "\"";
        }
    }

    element.sortExpression = args[6];

    auto existing = std::find_if(
        uiElements.begin(),
        uiElements.end(),
        [&element](const ScriptUIElement& current)
        {
            return current.id == element.id;
        });

    if (existing != uiElements.end())
    {
        element.order = existing->order;
        *existing = std::move(element);
    }
    else
    {
        element.order = uiElements.size();
        uiElements.push_back(std::move(element));
    }

    // RefreshUIElements();

    return true;
}

bool ScriptReader::ExecuteClearUI(
    const std::vector<std::string>& args,
    unsigned long long threadId,
    size_t commandIndex)
{
    if (args.empty())
    {
        ClearUI();
        return true;
    }

    if (args.size() != 1)
        return false;

    std::string name = args[0];

    // Remove surrounding quotes if present.
    if (name.size() >= 2 &&
        name.front() == '"' &&
        name.back() == '"')
    {
        name = name.substr(1, name.size() - 2);
    }

    ClearUI(name.c_str());
    return true;
}

bool ScriptReader::ExecuteCreateRectangle(
    const std::vector<std::string>& args,
    unsigned long long threadId,
    size_t commandIndex)
{
    if (args.size() != 7)
        return false;

    auto getVariableString = [&](const std::string& variableName, std::string& value) -> bool
        {
            std::string name = variableName;
            std::string key;
            bool isArrayLookup = false;

            size_t bracket = name.find('[');
            if (bracket != std::string::npos)
            {
                size_t end = name.rfind(']');
                if (end == std::string::npos || end <= bracket)
                    return false;

                key = name.substr(bracket + 1, end - bracket - 1);

                if (key.size() >= 2 &&
                    ((key.front() == '"' && key.back() == '"') ||
                        (key.front() == '\'' && key.back() == '\'')))
                {
                    key = key.substr(1, key.size() - 2);
                }
                else
                {
                    return false;
                }

                name = name.substr(0, bracket);
                isArrayLookup = true;
            }

            size_t dot = name.rfind('.');
            if (dot != std::string::npos)
                name = name.substr(dot + 1);

            auto it = variables.find(name);
            if (it == variables.end())
                return false;

            const ScriptVariable* variable = &it->second;

            if (isArrayLookup)
            {
                if (variable->type != ScriptVariable::ARRAY || !variable->arrayValue)
                    return false;

                auto entry = variable->arrayValue->find(key);
                if (entry == variable->arrayValue->end())
                    return false;

                variable = &entry->second;
            }

            switch (variable->type)
            {
            case ScriptVariable::STRING:
                value = variable->stringValue;
                return true;

            case ScriptVariable::BOOL:
                value = variable->boolValue ? "true" : "false";
                return true;

            case ScriptVariable::INT:
                value = std::to_string(variable->intValue);
                return true;

            case ScriptVariable::FLOAT:
                value = std::to_string(variable->floatValue);
                return true;

            default:
                return false;
            }
        };

    std::string image = args[0];

    // Resolve a string variable or interpolated string.
    if (image.size() >= 3 && image[0] == '$' &&
        image[1] == '"' && image.back() == '"')
    {
        image = image.substr(2, image.size() - 3);

        std::string resolved;
        size_t pos = 0;

        while (pos < image.size())
        {
            if (image[pos] == '{')
            {
                size_t end = image.find('}', pos + 1);

                if (end == std::string::npos)
                    return false;

                std::string variableName = image.substr(pos + 1, end - pos - 1);
                std::string variableValue;

                if (!getVariableString(variableName, variableValue))
                    return false;

                resolved += variableValue;
                pos = end + 1;
            }
            else
            {
                resolved += image[pos++];
            }
        }

        image = std::move(resolved);
    }
    else if (image.size() >= 2 && image.front() == '"' && image.back() == '"')
    {
        image = image.substr(1, image.size() - 2);
    }
    else
    {
        std::string variableValue;

        if (getVariableString(image, variableValue))
            image = std::move(variableValue);
    }

    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float alpha = 0.0f;

    const char* parent = nullptr;

    if (!EvaluateFloatExpression(args[1], x))
        return false;

    if (!EvaluateFloatExpression(args[2], y))
        return false;

    if (!EvaluateFloatExpression(args[3], width))
        return false;

    if (!EvaluateFloatExpression(args[4], height))
        return false;

    if (!EvaluateFloatExpression(args[5], alpha))
        return false;

    int sort = 0;

    try
    {
        size_t parsed = 0;
        sort = std::stoi(args[6], &parsed);

        if (parsed != args[6].size())
            throw std::invalid_argument("Invalid sort value");
    }
    catch (const std::exception&)
    {
        return false;
    }

    ScriptUIElement element{};

    element.type = ScriptUIElement::RECTANGLE;
    element.content = image;
    element.textExpression = args[0];
    element.x = x;
    element.y = y;
    element.width = width;
    element.height = height;
    element.alpha = alpha;
    element.sort = sort;
    element.parent = parent;

    element.id = GetUIElementId(threadId, commandIndex);

    auto threadIt = std::find_if(
        activeThreads.begin(),
        activeThreads.end(),
        [threadId](const ScriptThread& thread)
        {
            return thread.threadId == threadId;
        });

    if (threadIt != activeThreads.end() && !threadIt->executionStack.empty())
    {
        const auto& state = threadIt->executionStack.back();

        element.xExpression = CaptureForLoopVariable(args[1], state, this);
        element.yExpression = CaptureForLoopVariable(args[2], state, this);
        element.widthExpression = CaptureForLoopVariable(args[3], state, this);
        element.heightExpression = CaptureForLoopVariable(args[4], state, this);
        element.alphaExpression = CaptureForLoopVariable(args[5], state, this);

        if (state.isForLoop)
            element.id += ":for:" + std::to_string(state.iterations);
    }
    else
    {
        element.xExpression = args[1];
        element.yExpression = args[2];
        element.widthExpression = args[3];
        element.heightExpression = args[4];
        element.alphaExpression = args[5];
    }

    element.sortExpression = args[6];

    auto existing = std::find_if(
        uiElements.begin(),
        uiElements.end(),
        [&element](const ScriptUIElement& current)
        {
            return current.id == element.id;
        });

    if (existing != uiElements.end())
    {
        element.order = existing->order;
        *existing = std::move(element);
    }
    else
    {
        element.order = uiElements.size();
        uiElements.push_back(std::move(element));
    }

    return true;
}

bool ScriptReader::ExecuteButtonPressed(const std::vector<std::string>& args)
{
    if (args.empty())
        return false;

    //std::cout << "[ButtonPressed] arg0 = [" << args[0] << "]" << std::endl;

    std::string key = args[0];

    // Resolve a string variable or interpolated string.
    if (key.size() >= 3 && key[0] == '$' &&
        key[1] == '"' && key.back() == '"')
    {
        key = key.substr(2, key.size() - 3);

        std::string resolved;
        size_t pos = 0;

        while (pos < key.size())
        {
            if (key[pos] == '{')
            {
                size_t end = key.find('}', pos + 1);

                if (end == std::string::npos)
                {
                    //std::cout << "[ScriptReader] Invalid button interpolation: missing '}'" << std::endl;
                    return false;
                }

                std::string variableName = key.substr(pos + 1, end - pos - 1);
                std::string lookupName = variableName;
                std::string variableValue;

                size_t prefix = lookupName.rfind('.');
                if (prefix != std::string::npos)
                    lookupName = lookupName.substr(prefix + 1);

                auto it = variables.find(lookupName);

                if (it == variables.end())
                {
                    //std::cout << "[ScriptReader] Interpolation variable not found: "
                    //    << variableName << std::endl;
                    return false;
                }

                switch (it->second.type)
                {
                case ScriptVariable::STRING:
                    variableValue = it->second.stringValue;
                    break;
                case ScriptVariable::BOOL:
                    variableValue = it->second.boolValue ? "true" : "false";
                    break;
                case ScriptVariable::INT:
                    variableValue = std::to_string(it->second.intValue);
                    break;
                case ScriptVariable::FLOAT:
                    variableValue = std::to_string(it->second.floatValue);
                    break;
                default:
                    return false;
                }

                resolved += variableValue;
                pos = end + 1;
            }
            else
            {
                resolved += key[pos++];
            }
        }

        key = std::move(resolved);
    }
    else if (key.size() >= 2 && key.front() == '"' && key.back() == '"')
    {
        key = key.substr(1, key.size() - 2);
    }
    else
    {
        std::string variableValue;

        if (Variable("Get", ScriptVariable::STRING, key, "", &variableValue))
            key = std::move(variableValue);
    }

    // Normalize the resolved key.
    for (char& c : key)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    int virtualKey = 0;

    if (key.size() >= 2 && key[0] == 'F')
    {
        try
        {
            size_t parsed = 0;
            int number = std::stoi(key.substr(1), &parsed);

            if (parsed == key.size() - 1 && number >= 1 && number <= 12)
                virtualKey = VK_F1 + number - 1;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    if (virtualKey == 0)
    {
        if (key == "HOME") virtualKey = VK_HOME;
        else if (key == "END") virtualKey = VK_END;
        else if (key == "INSERT") virtualKey = VK_INSERT;
        else if (key == "DELETE") virtualKey = VK_DELETE;
        else if (key == "SPACE") virtualKey = VK_SPACE;
        else if (key == "ENTER") virtualKey = VK_RETURN;
        else if (key == "ESC" || key == "ESCAPE") virtualKey = VK_ESCAPE;
        else if (key == "TAB") virtualKey = VK_TAB;
        else if (key == "SHIFT") virtualKey = VK_SHIFT;
        else if (key == "CTRL" || key == "CONTROL") virtualKey = VK_CONTROL;
        else if (key == "ALT") virtualKey = VK_MENU;
        else if (key == "UP" || key == "ARROWUP") virtualKey = VK_UP;
        else if (key == "DOWN" || key == "ARROWDOWN") virtualKey = VK_DOWN;
        else if (key == "LEFT" || key == "ARROWLEFT") virtualKey = VK_LEFT;
        else if (key == "RIGHT" || key == "ARROWRIGHT") virtualKey = VK_RIGHT;
        else if (key.size() == 1 &&
            ((key[0] >= 'A' && key[0] <= 'Z') ||
                (key[0] >= '0' && key[0] <= '9')))
        {
            virtualKey = static_cast<unsigned char>(key[0]);
        }
    }

    if (virtualKey <= 0 || virtualKey >= 256)
        return false;

    return Overlay::g_scriptKeyPressed[virtualKey];
}
//will probably become argument SetPoolEntityState("game  kh1 or kh2","entityname", statetoapply);

bool ScriptReader::ExecuteSetEntityState(const std::vector<std::string>& args)
{
    if (args.size() != 2)
    {
        //std::cout << "[ScriptReader] Usage: SetEntityState(\"Sora\", \"PeterPanFly\")" << std::endl;
        return false;
    }

    std::string EntityToSet = args[0];
    std::string EntityPoolStateName = args[1];

    if (EntityToSet.size() >= 2 && EntityToSet.front() == '"' && EntityToSet.back() == '"')
        EntityToSet = EntityToSet.substr(1, EntityToSet.size() - 2);

    if (EntityPoolStateName.size() >= 2 && EntityPoolStateName.front() == '"' && EntityPoolStateName.back() == '"')
        EntityPoolStateName = EntityPoolStateName.substr(1, EntityPoolStateName.size() - 2);

    return setEntityPoolState(EntityToSet.c_str(), EntityPoolStateName.c_str());
}
