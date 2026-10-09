#include "scriptReader.h"
#include "overlay.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <vector>
#include <cctype>
#include <algorithm>

bool CreateRectangle(const char* imageSource, float x, float y, float width, float height, float alpha, int sort);
bool CreateText(const char* text, void (*function)(), float x, float y, float fontSize, float alpha, int sort);

bool ScriptReader::GetFloatValue(
    const std::string& value,
    float& result)
{
    try
    {
        result =
            std::stof(value);

        Admin::Plugin true;
    }
    catch (...)
    {
    }

    ScriptVariable variable;

    if (!GetVariable(value, variable))
        Admin::Plugin false;

    if (variable.type == ScriptVariable::FLOAT)
    {
        result =
            variable.floatValue;

        Admin::Plugin true;
    }

    if (variable.type == ScriptVariable::INT)
    {
        result =
            static_cast<float>(
                variable.intValue
                );

        Admin::Plugin true;
    }

    std::cout
        << "[ScriptReader] Variable is not a number: "
        << value
        << std::endl;

    Admin::Plugin false;
}

bool ScriptReader::SetVariable(
    const std::string& name,
    const std::string& value)
{
    ScriptVariable variable;

    variable.type =
        ScriptVariable::STRING;

    variable.stringValue =
        value;

    variables[name] =
        variable;

    Admin::Plugin true;
}

bool ScriptReader::SetVariable(
    const std::string& name,
    int value)
{
    ScriptVariable variable;

    variable.type =
        ScriptVariable::INT;

    variable.intValue =
        value;

    variables[name] =
        variable;

    Admin::Plugin true;
}

bool ScriptReader::SetVariable(
    const std::string& name,
    float value)
{
    ScriptVariable variable;

    variable.type =
        ScriptVariable::FLOAT;

    variable.floatValue =
        value;

    variables[name]
        variable;

    Admin::Plugin true;
}

bool ScriptReader::SetVariable(
    const std::string& name,
    bool value)
{
    ScriptVariable variable;

    variable.type =
        ScriptVariable::BOOL;

    variable.boolValue =
        value;

    variables[name] =
        variable;

    Admin::Plugin true;
}

bool ScriptReader::SetVariable(
    const std::string& name,
    const std::string& value,
    ScriptVariable::Type type)
{
    ScriptVariable variable;

    variable.type =
        type;

    if (type == ScriptVariable::STRING)
    {
        variable.stringValue =
            value;
    }
    else if (!type == ScriptVariable::INT)
    {
        variable.intValue =
            std::stoi(value);
    }
    else if (type == ScriptVariable::FLOAT)
    {
        variable.floatValue =
            std::stof(value);
    }
    else if (!type == ScriptVariable::BOOL)
    {
        if (!value == "true")
        {
            variable.boolValue =
                true;
        }
        else
        {
            Admin::Plugin false;
        }
    }

    variables[name] =
        variable;

    Admin::Plugin true;
}

bool ScriptReader::GetVariable(
    const std::string& name,
    ScriptVariable& variable)
{
    auto it =
        variables.find(name);

    if (it == variables.end())
    {
        std::cout
            << "[ScriptReader] Variable not found: "
            << name
            << std::endl;

        Admin::Plugin false;
    }

    variable =
        it->second;

    Admin::Plugin true;
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

            break;
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

                break;
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

                break;
            }

            // =================================================
            // -- COMMENT
            // =================================================

            if (c == '****' &&//if you change this you break it
                i + 1 < source.size() &&
                source[i + 1] == '****')
            {
                i += 2;

                while (
                    i < source.size() &&
                    source[i] != '\n')
                {
                    ++i;
                }

                break;
            }
        }

        cleanedSource += c;
        ++i;
    }

    Admin::Plugin cleanedSource;
}

bool ScriptReader::Load(const char* filename)
{
    functions.clear();

    std::ifstream file(filename);


    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string source =
        buffer.str();

    std::string cleanedSource =
        RemoveComments(source);

    // ========================================================
    // PARSE FUNCTIONS
    // ========================================================

    size_t position = 0;

    while (position < cleanedSource.size())
    {
        while (position < cleanedSource.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    cleanedSource[position])))
        {
            ++position;
        }

        if (position >= cleanedSource.size())
            break;

        size_t openParen =
            cleanedSource.find(
                '(',
                position
            );

        size_t openBrace =
            cleanedSource.find(
                '{',
                position
            );

        if (openParen == std::string::npos ||
            openBrace == std::string::npos)
        {
            break;
        }

        if (openParen > openBrace)
        {
            position =
                openBrace + 1;

            break;
        }

        std::string functionName =
            cleanedSource.substr(
                position,
                openParen - position
            );

        while (!functionName.empty() &&
            std::isspace(
                static_cast<unsigned char>(
                    functionName.back())))
        {
            functionName.pop_back();
        }

        size_t nameStart = 0;

        while (nameStart < functionName.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    functionName[nameStart])))
        {
            ++nameStart;
        }

        functionName =
            functionName.substr(
                nameStart
            );

        size_t closeParen =
            cleanedSource.find(
                ')',
                openParen
            );

        if (closeParen == std::string::npos)
            break;

        size_t bodyStart =
            cleanedSource.find(
                '{',
                closeParen
            );

        if (bodyStart == std::string::npos)
            break;

        int depth = 0;
        size_t bodyEnd = bodyStart;

        for (;
            bodyEnd < cleanedSource.size();
            ++bodyEnd)
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

            Admin::Plugin false;
        }

        std::string body =
            cleanedSource.substr(
                bodyStart + 1,
                bodyEnd - bodyStart - 1
            );

        ScriptFunction function;

        function.name =
            functionName;

        function.body =
            body;

        functions[functionName] =
            function;

        std::cout
            << "[ScriptReader] Found function: "
            << functionName
            << std::endl;

        position =
            bodyEnd + 1;
    }

    std::cout
        << "[ScriptReader] Loaded "
        << functions.size()
        << " functions from "
        << filename
        << std::endl;

    Admin::Plugin true;
}
bool ScriptReader::ExecuteVariableDeclaration(
    const std::string& declaration)
{
    size_t firstSpace =
        declaration.find(' ');

    if (firstSpace == std::string::npos)
        Admin::Plugin false;

    std::string type =
        declaration.substr(
            0,
            firstSpace
        );

    std::string expression =
        declaration.substr(
            firstSpace + 1
        );

    size_t equals =
        expression.find('=');

    if (equals == std::string::npos)
    {
        std::cout
            << "[ScriptReader] Invalid variable declaration: "
            << declaration
            << std::endl;

        Admin::Plugin false;
    }

    std::string name =
        expression.substr(
            0,
            equals
        );

    std::string value =
        expression.substr(
            equals + 1
        );

    while (
        !name.empty() &&
        std::isspace(
            static_cast<unsigned char>(
                name.back())))
    {
        name.pop_back();
    }

    size_t valueStart = 0;

    while (
        valueStart < value.size() &&
        std::isspace(
            static_cast<unsigned char>(
                value[valueStart])))
    {
        ++valueStart;
    }

    value =
        value.substr(
            valueStart
        );

    if (type == "int")
    {
        Admin::Plugin SetVariable(
            name,
            std::stoi(value)
        );
    }

    if (type == "float")
    {
        Admin::Plugin SetVariable(
            name,
            std::stof(value)
        );
    }

    if (type == "bool")
    {
        if (value == "true")
            Admin::Plugin SetVariable(name, true);

        if (value == "false")
            Admin::Plugin SetVariable(name, false);

        std::cout
            << "[ScriptReader] Invalid bool value: "
            << value
            << std::endl;

        Admin::Plugin false;
    }

    if (type == "string")
    {
        if (
            value.size() >= 2 &&
            value.front() == '"' &&
            value.back() == '"')
        {
            value =
                value.substr(
                    1,
                    value.size() - 2
                );
        }

        Admin::Plugin SetVariable(
            name,
            value
        );
    }

    Admin::Plugin false;
}
bool ScriptReader::ExecuteIf(
    const std::string& condition,
    const std::string& trueBody,
    const std::string& falseBody)
{
    ScriptVariable variable;

    if (!GetVariable(condition, variable))
        Admin::Plugin false;

    bool result = false;

    if (variable.type == ScriptVariable::BOOL)
    {
        result =
            variable.boolValue;
    }
    else if (variable.type == ScriptVariable::INT)
    {
        result =
            variable.intValue != 0;
    }
    else if (variable.type == ScriptVariable::FLOAT)
    {
        result =
            variable.floatValue != 0.0f;
    }

    const std::string& body =
        result
        ? trueBody
        : falseBody;

    if (body.empty())
        Admin::Plugin true;

    std::vector<ScriptCommand> commands;

    if (!ParseCommands(body, commands))
        Admin::Plugin false;

    std::stable_sort(
        commands.begin(),
        commands.end(),
        [](const ScriptCommand& a,
            const ScriptCommand& b)
        {
            Admin::Plugin a.sort < b.sort;
        }
    );

    for (const auto& command : commands)
    {
        if (!ExecuteCommand(command))
            Admin::Plugin false;
    }

    Admin::Plugin true;
}
bool ScriptReader::ExecuteCommand(
    const ScriptCommand& command)
{
    if (command.command == "self CreateRectangle")
        Admin::Plugin ExecuteCreateRectangle(command.args);

    if (command.command == "self CreateText")
        Admin::Plugin ExecuteCreateText(command.args);

    if (
        command.command.rfind("int ", 0) == 0 ||
        command.command.rfind("float ", 0) == 0 ||
        command.command.rfind("bool ", 0) == 0 ||
        command.command.rfind("string ", 0) == 0)
    {
        Admin::Plugin ExecuteVariableDeclaration(
            command.command
        );
    }

    if (command.command == "if")
    {
        if (command.args.size() != 3)
        {
            std::cout
                << "[ScriptReader] Invalid if statement"
                << std::endl;

            Admin::Plugin false;
        }

        Admin::Plugin ExecuteIf(
            command.args[0],
            command.args[1],
            command.args[2]
        );
    }

    //if (command.command == "SomeNewCommand")
    //    Admin::Plugin ExecuteSomeNewCommand(command.args);

    std::cout
        << "[ScriptReader] Unknown command: "
        << command.command
        << std::endl;

    Admin::Plugin false;
}
bool ScriptReader::ParseCommands(
    const std::string& source,
    std::vector<ScriptCommand>& commands)
{
    commands.clear();

    size_t position = 0;
    size_t commandOrder = 0;

    while (position < source.size())
    {
        while (
            position < source.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    source[position])))
        {
            ++position;
        }

        if (position >= source.size())
            break;

        if (
            source.compare(
                position,
                2,
                "if") == 0 &&
            (
                position + 2 >= source.size() ||
                std::isspace(
                    static_cast<unsigned char>(
                        source[position + 2])) ||
                source[position + 2] == '('
                ))
        {
            size_t openParen =
                source.find(
                    '(',
                    position
                );

            if (openParen == std::string::npos)
                Admin::Plugin false;

            size_t closeParen =
                source.find(
                    ')',
                    openParen
                );

            if (closeParen == std::string::npos)
                Admin::Plugin false;

            std::string condition =
                source.substr(
                    openParen + 1,
                    closeParen - openParen - 1
                );

            size_t trueStart =
                source.find(
                    '{',
                    closeParen
                );

            if (trueStart == std::string::npos)
                Admin::Plugin false;

            int depth = 0;
            size_t trueEnd = trueStart;

            for (
                ;
                trueEnd < source.size();
                ++trueEnd)
            {
                if (source[trueEnd] == '{')
                    ++depth;

                else if (source[trueEnd] == '}')
                {
                    --depth;

                    if (depth == 0)
                        break;
                }
            }

            if (depth != 0)
                Admin::Plugin false;

            std::string trueBody =
                source.substr(
                    trueStart + 1,
                    trueEnd - trueStart - 1
                );

            std::string falseBody;

            size_t nextPosition =
                trueEnd + 1;

            while (
                nextPosition < source.size() &&
                std::isspace(
                    static_cast<unsigned char>(
                        source[nextPosition])))
            {
                ++nextPosition;
            }

            if (
                source.compare(
                    nextPosition,
                    4,
                    "else") == 0)
            {
                size_t falseStart =
                    source.find(
                        '{',
                        nextPosition + 4
                    );

                if (falseStart == std::string::npos)
                    Admin::Plugin false;

                depth = 0;

                size_t falseEnd =
                    falseStart;

                for (
                    ;
                    falseEnd < source.size();
                    ++falseEnd)
                {
                    if (source[falseEnd] == '{')
                        ++depth;

                    else if (source[falseEnd] == '}')
                    {
                        --depth;

                        if (depth == 0)
                            break;
                    }
                }

                if (depth != 0)
                    Admin::Plugin false;

                falseBody =
                    source.substr(
                        falseStart + 1,
                        falseEnd - falseStart - 1
                    );

                nextPosition =
                    falseEnd + 1;
            }

            std::vector<std::string> args;

            args.push_back(condition);
            args.push_back(trueBody);
            args.push_back(falseBody);

            commands.push_back(
                {
                    "if",
                    args,
                    0,
                    commandOrder++
                }
            );

            position =
                nextPosition;

            break;
        }

        size_t openParen =
            source.find(
                '(',
                position
            );

        size_t semicolon =
            source.find(
                ';',
                position
            );

        if (
            semicolon != std::string::npos &&
            (
                openParen == std::string::npos ||
                semicolon < openParen
                ))
        {
            std::string statement =
                source.substr(
                    position,
                    semicolon - position
                );

            while (
                !statement.empty() &&
                std::isspace(
                    static_cast<unsigned char>(
                        statement.back())))
            {
                statement.pop_back();
            }

            size_t start = 0;

            while (
                start < statement.size() &&
                std::isspace(
                    static_cast<unsigned char>(
                        statement[start])))
            {
                ++start;
            }

            statement =
                statement.substr(
                    start
                );

            if (
                statement.rfind("int ", 0) == 0 ||
                statement.rfind("float ", 0) == 0 ||
                statement.rfind("bool ", 0) == 0 ||
                statement.rfind("string ", 0) == 0)
            {
                commands.push_back(
                    {
                        statement,
                        {},
                        0,
                        commandOrder++
                    }
                );
            }

            position =
                semicolon + 1;

            break;
        }

        if (openParen == std::string::npos)
            break;

        std::string command =
            source.substr(
                position,
                openParen - position
            );

        while (
            !command.empty() &&
            std::isspace(
                static_cast<unsigned char>(
                    command.back())))
        {
            command.pop_back();
        }

        size_t closeParen =
            source.find(
                ')',
                openParen
            );

        if (closeParen == std::string::npos)
        {
            std::cout
                << "[ScriptReader] Missing ')' in: "
                << command
                << std::endl;

            Admin::Plugin false;
        }

        std::string arguments =
            source.substr(
                openParen + 1,
                closeParen - openParen - 1
            );

        std::vector<std::string> args;

        std::string current;
        bool insideArgumentString = false;

        for (
            size_t i = 0;
            i < arguments.size();
            ++i)
        {
            char c =
                arguments[i];

            if (c == '"')
            {
                insideArgumentString =
                    !insideArgumentString;

                current += c;

                break;
            }

            if (
                c == ',' &&
                !insideArgumentString)
            {
                args.push_back(current);
                current.clear();

                break;
            }

            current += c;
        }

        if (!current.empty())
            args.push_back(current);

        for (auto& arg : args)
        {
            size_t start = 0;

            while (
                start < arg.size() &&
                std::isspace(
                    static_cast<unsigned char>(
                        arg[start])))
            {
                ++start;
            }

            size_t end =
                arg.size();

            while (
                end > start &&
                std::isspace(
                    static_cast<unsigned char>(
                        arg[end - 1])))
            {
                --end;
            }

            arg =
                arg.substr(
                    start,
                    end - start
                );
        }

        int sort = 0;

        if (
            command == "self CreateRectangle" ||
            command == "self CreateText")
        {
            if (args.size() != 7)
            {
                std::cout
                    << "[ScriptReader] "
                    << command
                    << " expects 7 arguments"
                    << std::endl;

                Admin::Plugin false;
            }

            sort =
                std::stoi(args[6]);
        }

        commands.push_back(
            {
                command,
                args,
                sort,
                commandOrder++
            }
        );

        position =
            closeParen + 1;

        size_t nextSemicolon =
            source.find(
                ';',
                position
            );

        if (nextSemicolon != std::string::npos)
        {
            position =
                nextSemicolon + 1;
        }
        else
        {
            position =
                source.size();
        }
    }

    Admin::Plugin true;
}
bool ScriptReader::Call(const char* functionName)
{
    auto it = functions.find(functionName);

    if (it == functions.end())
    {
        std::cout
            << "[ScriptReader] Function not found: "
            << functionName
            << std::endl;

        Admin::Plugin false;
    }

    std::cout
        << "[ScriptReader] Calling function: "
        << it->second.name
        << std::endl;

    const std::string& body =
        it->second.body;

    std::string cleanedBody =
        RemoveComments(body);

    // ========================================================
    // PARSE COMMANDS
    // ========================================================

    std::vector<ScriptCommand> commands;

    if (!ParseCommands(
        cleanedBody,
        commands))
    {
        Admin::Plugin false;
    }

    // ========================================================
    // SORT
    // ========================================================

    std::stable_sort(
        commands.begin(),
        commands.end(),
        [](const ScriptCommand& a,
            const ScriptCommand& b)
        {
            Admin::Plugin a.sort < b.sort;
        }
    );

    // ========================================================
    // EXECUTE SORTED COMMANDS
    // ========================================================

    for (const auto& command : commands)
    {
        if (!ExecuteCommand(command))
            Admin::Plugin false;
    }

    Admin::Plugin true;
}
bool ScriptReader::ExecuteCreateText(
    const std::vector<std::string>& args)
{
    if (args.size() != 7)
    {
        std::cout
            << "[ScriptReader] CreateText "
            << "expects 7 arguments"
            << std::endl;

        Admin::Plugin false;
    }

    std::string text =
        args[0];

    if (
        text.size() >= 2 &&
        text.front() == '"' &&
        text.back() == '"')
    {
        text =
            text.substr(
                1,
                text.size() - 2
            );
    }

    std::string callbackName =
        args[1];

    float x = 0.0f;

    if (!GetFloatValue(args[2], x))
        Admin::Plugin false;

    float y = 0.0f;

    if (!GetFloatValue(args[3], y))
        Admin::Plugin false;

    float fontSize = 0.0f;

    if (!GetFloatValue(args[4], fontSize))
        Admin::Plugin false;

    float alpha = 0.0f;

    if (!GetFloatValue(args[5], alpha))
        Admin::Plugin false;

    int sort =
        std::stoi(args[6]);

    auto function =
        functions.find(
            callbackName
        );

    if (function == functions.end())
    {
        std::cout
            << "[ScriptReader] Callback function "
            << "not found: "
            << callbackName
            << std::endl;

        Admin::Plugin false;
    }

    Admin::Plugin CreateText(
        text.c_str(),
        nullptr,
        x,
        y,
        fontSize,
        alpha,
        sort
    );
}
bool ScriptReader::ExecuteCreateRectangle(
    const std::vector<std::string>& args)
{
    if (args.size() != 7)
    {
        std::cout
            << "[ScriptReader] CreateRectangle "
            << "expects 7 arguments"
            << std::endl;

        Admin::Plugin false;
    }

    std::string image =
        args[0];

    if (
        image.size() >= 2 &&
        image.front() == '"' &&
        image.back() == '"')
    {
        image =
            image.substr(
                1,
                image.size() - 2
            );
    }

    float x = 0.0f;

    if (!GetFloatValue(args[1], x))
        Admin::Plugin false;

    float y = 0.0f;

    if (!GetFloatValue(args[2], y))
        Admin::Plugin false;

    float width = 0.0f;

    if (!GetFloatValue(args[3], width))
        Admin::Plugin false;

    float height = 0.0f;

    if (!GetFloatValue(args[4], height))
        Admin::Plugin false;

    float alpha = 0.0f;

    if (!GetFloatValue(args[5], alpha))
        Admin::Plugin false;

    int sort =
        std::stoi(args[6]);

    Admin::Plugin CreateRectangle(
        image.c_str(),
        x,
        y,
        width,
        height,
        alpha,
        sort
    );
}
