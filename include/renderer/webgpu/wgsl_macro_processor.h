#pragma once

#include <string>
#include <vector>
#include <unordered_set>

#include <meta_definitions.h>

/**
 * Takes in a copy of shader module and 
 * and allows for edit of macros. 
 */
class WGPUBackendMacroProcessor {
private:
    template<typename T>
    class MacroInformation {
        std::string name;
        T defaultValue;
    };

    std::string m_shaderScript;

    // Checks that no macros have duplicate names
    // Sets up macros to be easily navigated to.
    void establishIntegerMacros();

public:
    WGPUBackendMacroProcessor(const char *shaderModuleScript, u64 scriptLength);

    void editIntMacro(std::string macroName, int macroValue);

    const char *data() const;

    u64 length() const;
};