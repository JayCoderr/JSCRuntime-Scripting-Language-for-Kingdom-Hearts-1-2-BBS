#pragma once

#include "scriptreader.h"

class menuStructure
{
public:
    static ScriptReader scriptReader;

    static bool Load(const char* filename)
    {
        return scriptReader.Load(filename);
    }

    static bool Call(const char* functionName)
    {
        return scriptReader.Call(functionName);
    }
};