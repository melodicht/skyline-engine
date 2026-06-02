#include "wgsl_macro_processor.h"

#include <regex>

void WGPUBackendMacroProcessor::establishIntegerMacros() {
    // Ensures that integer macro comment blocks don't already exist
    auto leftIntegerMacroIndicator = m_shaderScript.find("/*[<int><");
    auto rightIntegerMacroIndicator = m_shaderScript.find("/*)<int><");
    ASSERT_PRINT(leftIntegerMacroIndicator == std::string::npos, "Macro comment block structure exists in script already");
    ASSERT_PRINT(rightIntegerMacroIndicator == std::string::npos, "Macro comment block structure exists in script already");

    // Begins finding base macro blocks
    // In format of [<int><MacroName><DefaultValue>]
    std::regex integerMacroBlockPattern("\\[<int><([^<>]+)><([^<>]+)>\\]");
    std::smatch macroMatch;
    while (std::regex_search(m_shaderScript, macroMatch, integerMacroBlockPattern)) {
        std::string macroName = macroMatch[1];
        std::string defaultValueString = macroMatch[2];

        std::string macroScript = "/*[<int><" + macroName + ">(*/" + defaultValueString + "/*)<int><" + macroName + ">]*/";
        m_shaderScript.replace(macroMatch[0].first, macroMatch[0].second, macroScript);
    }
}

WGPUBackendMacroProcessor::WGPUBackendMacroProcessor(const char *shaderModuleScript, u64 scriptLength) {
    // Creates copy
    m_shaderScript = std::string(shaderModuleScript, scriptLength);

    // Establish integer macro blocks
    establishIntegerMacros();
}

void WGPUBackendMacroProcessor::editIntMacro(std::string macroName, int macroValue) {
    std::string leftMacroScript = "/*[<int><" + macroName + ">(*/";
    std::string rightMacroScript = "/*)<int><" + macroName + ">]*/";
    std::string replaceValue = std::to_string(macroValue);
    auto leftScriptLocation = m_shaderScript.find(leftMacroScript);
    auto rightScriptLocation = m_shaderScript.find(rightMacroScript);
    if (leftScriptLocation == std::string::npos || rightScriptLocation == std::string::npos || leftScriptLocation > rightScriptLocation) {
        LOG_ERROR("Macro attempted to be edited can't be found");
    }
    else {
        m_shaderScript.replace(
            leftScriptLocation + leftMacroScript.length(), 
            rightScriptLocation - (leftScriptLocation + leftMacroScript.length()), 
            replaceValue);
    }
}

const char * WGPUBackendMacroProcessor::data() const {
    return m_shaderScript.data();
}

u64 WGPUBackendMacroProcessor::length() const {
    return m_shaderScript.length();
}