#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "includes.h"

namespace fs = std::filesystem;

struct DefineMacro
{
    std::string name;
    std::string text;
};

struct CondFrame
{
    bool isActive;          
    bool anyTaken;          
    bool inIfDef;         
};

struct PreProcContext
{
    fs::path exeDir;                       
    std::unordered_set<std::string> included; 
    std::vector<DefineMacro> defines;       
    std::unordered_set<std::string> inlineSymbols; 
    std::stack<CondFrame> condStack;        
    std::stack<std::string> classStack;     
    bool skipOutput = false;                
    std::stack<std::unordered_set<std::string>> classPrivateSetStack;
    // 查找#define宏
    DefineMacro* findDefine(const std::string& name)
    {
        for(auto& m : defines)
        {
            if(m.name == name) return &m;
        }
        return nullptr;
    }
    bool isDefined(const std::string& name)
    {
        return findDefine(name) != nullptr || inlineSymbols.count(name);
    }
    bool isCurrentClassPrivateIdent(const std::string& ident) const
    {
        if(classPrivateSetStack.empty()) return false;
        return classPrivateSetStack.top().count(ident) > 0;
    }
};

struct PreProcLineResult
{
    std::string outLine;
    bool consumeRestOfInput = false; 
};

PreProcLineResult preproc_handle_line(const std::string& line, PreProcContext& ctx);

void preproc_include_file(PreProcContext& ctx, const fs::path& baseDir, const std::string& filename, std::string& outSource);

std::string preprocess_source(const std::string& rawSource, const fs::path& exePath);

std::string ltrim(const std::string &s);
std::string strip_hash_directive(const std::string& line);

std::string rewrite_private_ident(PreProcContext& ctx, const std::string& ident);
std::string rewrite_private_in_line(PreProcContext& ctx, const std::string& line);
#endif