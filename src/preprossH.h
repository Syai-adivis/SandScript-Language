#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "includes.h"

namespace fs = std::filesystem;

// 文本宏：#define NAME text ... #enddef
struct DefineMacro
{
    std::string name;
    std::string text;
};

// 条件编译状态栈每一层
struct CondFrame
{
    bool isActive;          // 当前分支是否处于生效输出
    bool anyTaken;          // 是否已经有分支被选中（#elif不再生效）
    bool inIfDef;           // 是否是ifdef/ifndef块
};

// 预处理器上下文
struct PreProcContext
{
    fs::path exeDir;                        // 解释器exe所在目录，#from不指定sysdir时的基准
    std::unordered_set<std::string> included; // 已经include过的文件（绝对路径小写）防循环
    std::vector<DefineMacro> defines;       // #define 文本宏表
    std::unordered_set<std::string> inlineSymbols; // #inline 常量符号标记（仅记录名字）
    std::stack<CondFrame> condStack;        // 条件编译栈
    std::stack<std::string> classStack;     // 当前正在解析的类名栈，用于#private
    bool skipOutput = false;                // 全局是否跳过输出（条件编译关闭时）

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
};

// 单行预处理结果
struct PreProcLineResult
{
    std::string outLine;
    bool consumeRestOfInput = false; // #enddef 需要吃掉多行直到#enddef
};

// 解析一行（行首允许空白），返回处理后行，修改ctx状态
PreProcLineResult preproc_handle_line(const std::string& line, PreProcContext& ctx);

// 递归include文件，把文件内容追加到输出
void preproc_include_file(PreProcContext& ctx, const fs::path& baseDir, const std::string& filename, std::string& outSource);

// 【主入口】
// rawSource：原始输入源码
// exePath：解释器可执行文件完整路径，用于 #from include 基准目录
// 返回经过include、宏展开、条件编译、#private处理之后的源码文本
std::string preprocess_source(const std::string& rawSource, const fs::path& exePath);

// 工具：去除行首空白
std::string ltrim(const std::string &s);
// 工具：分割预处理指令，返回#后面的全部内容（去掉#）
std::string strip_hash_directive(const std::string& line);
// 工具：替换#private伪私有字段：在类内，标识符name → __ClassName__name
std::string rewrite_private_ident(PreProcContext& ctx, const std::string& ident);

#endif