#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <chrono>

struct ScriptFunction
{
    std::string name;
    std::string body;
    std::vector<std::string> parameters;
};

struct ScriptCommand
{
    std::string command;
    std::vector<std::string> args;

    // IF CONDITION
    std::string condition;
    std::string trueBody;
    std::string falseBody;

    // COMMAND ORDER
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
        BOOL,
        ARRAY,
        UNDEFINED
    };

    Type type = INT;

    std::string stringValue;
    int intValue = 0;
    float floatValue = 0.0f;
    bool boolValue = false;

    std::shared_ptr<std::unordered_map<std::string, ScriptVariable>> arrayValue;

    ScriptVariable() : arrayValue(nullptr) {}
};

struct ScriptExecutionState
{
    std::vector<ScriptCommand> commands;
    size_t commandIndex = 0;

    // FOR LOOP STATE
    bool isForLoop = false;
    std::string initializer;
    std::string condition;
    std::string increment;
    int iterations = 0;

    // NESTED WAIT STATE
    bool waiting = false;
    std::chrono::steady_clock::time_point waitUntil{};

    bool checkForCondition = true;
};

struct ScriptThread
{
    std::string functionName;
    std::vector<ScriptCommand> commands;
    std::vector<std::string> arguments;

    // Parameter name -> original script variable name.
    std::unordered_map<std::string, std::string> parameterAliases;

    size_t commandIndex = 0;
    float waitTime = 0.0f;

    bool active = true;
    bool chained = false;
    bool restartRequested = false;
    bool loop = false;

    unsigned long long threadId = 0;

    std::chrono::steady_clock::time_point waitUntil{};
    bool waiting = false;

    std::vector<ScriptExecutionState> executionStack;
};

enum class UIUpdateMode
{
    UPDATE,
    CLEAR
};

struct ScriptUIElement
{
    enum Type
    {
        RECTANGLE,
        TEXT
    };

    Type type = RECTANGLE;

    std::string id;
    std::string content;
    std::string textExpression;
    std::string callbackName;
    std::string xExpression;
    std::string yExpression;
    std::string widthExpression;
    std::string heightExpression;
    std::string alphaExpression;
    std::string sortExpression;

    std::string parent;

    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float fontSize = 0.0f;
    float alpha = 1.0f;

    int sort = 0;
    size_t order = 0;
};

class ScriptReader
{
public:
    bool Load(const char* filename);
    
    bool StartThread(
        const std::string& functionName,
        bool loop,
        const std::vector<std::string>& arguments = {});

    bool Call(
        const char* functionName,
        const std::vector<std::string>& arguments = {});

    void UpdateThreads();
    
    static constexpr size_t INVALID_THREAD_INDEX =
        static_cast<size_t>(-1);

    std::vector<ScriptUIElement> uiElements;

    void ClearUI(const char* name = nullptr)
    {
        if (!name || name[0] == '\0')
        {
            uiElements.clear();
            return;
        }

        const std::string target(name);

        uiElements.erase(
            std::remove_if(
                uiElements.begin(),
                uiElements.end(),
                [&target](const ScriptUIElement& element)
                {
                    return !element.parent.empty() &&
                        element.parent == target;
                }
            ),
            uiElements.end()
        );
    }
    bool ExecuteButtonPressed(const std::vector<std::string>& args);  
    bool ExecuteSetEntityState(const std::vector<std::string>& args);  
    bool EvaluateFloatExpression(const std::string& expression, float& result);
    bool IsArithmeticExpression(const std::string& expression);
    bool ExecuteForLoop(
        const std::string& initializer,
        const std::string& condition,
        const std::string& increment,
        const std::vector<ScriptCommand>& body,
        unsigned long long threadId,
        size_t commandIndex);
    
    bool ExecuteClearUI(
        const std::vector<std::string>& args,
        unsigned long long threadId,
        size_t commandIndex);

    bool ResolveTextExpression(const std::string& expression, std::string& result) const;

    void RefreshUIElements();

    bool Variable(
        const std::string& operation,
        ScriptVariable::Type type,
        const std::string& name,
        std::string value,
        void* result);

private:
    int currentThreadIndex = -1;

    std::string GetUIElementId(
        unsigned long long threadId,
        size_t commandIndex) const;

    void CancelThread(const std::string& functionName);
    void CancelThreadById(unsigned long long threadId);

    bool EvaluateCondition(
        const std::string& condition,
        bool& result
    );

    bool ExecuteIf(
        const std::string& condition,
        const std::string& trueBody,
        const std::string& falseBody,
        bool* chained,
        unsigned long long threadId,
        size_t commandIndex);

    bool ParseCommands(
        const std::string& source,
        std::vector<ScriptCommand>& commands
    );

    bool ExecuteVariableDeclaration(
        const std::string& declaration,
        unsigned long long threadId,
        size_t commandIndex
    );
    
    bool ExecuteCommand(
        const ScriptCommand& command,
        bool* chained,
        unsigned long long threadId,
        size_t commandIndex);

    bool ExecuteCreateRectangle(
        const std::vector<std::string>& args,
        unsigned long long threadId,
        size_t commandIndex
    );

    bool ExecuteCreateText(
        const std::vector<std::string>& args,
        unsigned long long threadId,
        size_t commandIndex
    );

    std::string RemoveComments(
        const std::string& source
    );

    std::unordered_map<
        std::string,
        ScriptVariable
    > variables;

    std::unordered_map<std::string, ScriptFunction> functions;
};
