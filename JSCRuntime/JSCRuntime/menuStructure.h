#pragma once

#include "scriptreader.h"

class menuStructure
{
public:
    static ScriptReader scriptReader;

    static bool Load(
        const char* filename,
        bool hotReload = false)
    {
        return scriptReader.Load(
            filename
        );
    }

    static bool Call(const char* functionName)
    {
        return scriptReader.Call(functionName);
    }

    static void UpdateThreads()
    {
        scriptReader.UpdateThreads();
    }
};
