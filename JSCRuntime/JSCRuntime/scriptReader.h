#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct ScriptFunction
{
    std::string name;
    std::string body;
};

struct ScriptCommand
{
    std::string command;
    std::vector<std::string> args;
    int sort = 0;
    size_t order = 0;
};

struct ScriptVariable
{
    enum Type
    {
        STRING,
        INT,
        FLOAT,
        BOOL
    };

    Type type = INT;

    std::string stringValue;
    int intValue = 0;
    float floatValue = 0.0f;
    bool boolValue = false;
};

class ScriptReader
{
public:
    bool Load(const char* filename);
    bool Call(const char* functionName);

private:
    bool ParseCommands(
        const std::string& source,
        std::vector<ScriptCommand>& commands
    );

    bool ExecuteIf(
        const std::string& condition,
        const std::string& trueBody,
        const std::string& falseBody
    );

    bool GetFloatValue(
        const std::string& value,
        float& result
    );

    bool ExecuteVariableDeclaration(
        const std::string& declaration
    );
    
    bool ExecuteCommand(
        const ScriptCommand& command
    );

    bool ExecuteCreateRectangle(
        const std::vector<std::string>& args
    );

    bool ExecuteCreateText(
        const std::vector<std::string>& args
    );

    std::string RemoveComments(
        const std::string& source
    );

    bool SetVariable(
        const std::string& name,
        const std::string& value
    );

    bool SetVariable(
        const std::string& name,
        int value
    );

    bool SetVariable(
        const std::string& name,
        float value
    );

    bool SetVariable(
        const std::string& name,
        bool value
    );

    bool SetVariable(
        const std::string& name,
        const std::string& value,
        ScriptVariable::Type type
    );

    bool GetVariable(
        const std::string& name,
        ScriptVariable& variable
    );

    std::unordered_map<
        std::string,
        ScriptVariable
    > variables;

    std::unordered_map<std::string, ScriptFunction> functions;
};