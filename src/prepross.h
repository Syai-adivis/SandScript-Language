#include "preprossH.h"
#include <sstream>
#include <cctype>

std::string ltrim(const std::string &s)
{
    size_t start = 0;
    while(start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        start++;
    return s.substr(start);
}

std::string strip_hash_directive(const std::string& line)
{
    std::string t = ltrim(line);
    if(!t.empty() && t[0] == '#')
        return t.substr(1);
    return "";
}

std::string rewrite_private_ident(PreProcContext& ctx, const std::string& ident)
{
    if(ctx.classStack.empty())
        return ident;
    const std::string& cls = ctx.classStack.top();
    return "__" + cls + "__" + ident;
}

static std::string expand_macros_in_line(PreProcContext& ctx, std::string line)
{
    for(auto& def : ctx.defines)
    {
        size_t pos = 0;
        while(pos < line.size())
        {
            auto idx = line.find(def.name, pos);
            if(idx == std::string::npos) break;
            bool okLeft = (idx == 0) || (!std::isalnum(static_cast<unsigned char>(line[idx-1])) && line[idx-1]!='_');
            bool okRight = (idx+def.name.size() >= line.size()) || (!std::isalnum(static_cast<unsigned char>(line[idx+def.name.size()])) && line[idx+def.name.size()]!='_');
            if(okLeft && okRight)
            {
                line.replace(idx, def.name.size(), def.text);
                pos = idx + def.text.size();
            }
            else
            {
                pos = idx + 1;
            }
        }
    }
    return line;
}

static bool starts_with(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

PreProcLineResult preproc_handle_line(const std::string& line, PreProcContext& ctx)
{
    PreProcLineResult res;
    std::string hashBody = strip_hash_directive(line);
    if(hashBody.empty())
    {
        if(!ctx.skipOutput)
        {
            std::string expanded = expand_macros_in_line(ctx, line);
            std::string priv_rewritten = rewrite_private_in_line(ctx, expanded);
            res.outLine = priv_rewritten + "\n";
        }
        return res;
    }
    std::istringstream iss(hashBody);
    std::string cmd;
    iss >> cmd;

    // ========== #from xxx include file.sand ==========
    if(cmd == "from")
    {
        if(ctx.skipOutput) return res;
        std::string sysDir;
        std::string kw;
        iss >> sysDir >> kw;
        if(kw != "include") return res;
        std::string fname;
        iss >> fname;
        fs::path base;
        if(!sysDir.empty() && sysDir != ";")
        {
            base = fs::path(sysDir);
        }
        else
        {
            base = ctx.exeDir;
        }
        preproc_include_file(ctx, base, fname, res.outLine);
        return res;
    }
    // ========== #include ==========
    if(cmd == "include")
    {
        if(ctx.skipOutput) return res;
        std::string fname;
        iss >> fname;
        preproc_include_file(ctx, ctx.exeDir, fname, res.outLine);
        return res;
    }
    // ========== #private  ==========
    if(cmd == "private")
    {
        if(ctx.skipOutput) return res;
            if(ctx.classPrivateSetStack.empty())
        {
            std::cerr << "[Preproc] error: #private must be used inside class begin ... end\n";
            return res;
        }
        std::string rest;
        std::getline(iss, rest);
        std::string trimmed = ltrim(rest);
        if(!trimmed.empty() && trimmed.back() == ';') trimmed.pop_back();
        trimmed = ltrim(trimmed);
        std::string outStmt;

        auto& privSet = ctx.classPrivateSetStack.top();

        size_t p=0;
        while(p < trimmed.size())
        {
            size_t cpos = trimmed.find(',',p);
            std::string var;
            if(cpos == std::string::npos)
            {
                var = trimmed.substr(p);
                p = trimmed.size();
            }
            else
            {
                var = trimmed.substr(p, cpos-p);
                p = cpos+1;
            }
            var = ltrim(var);
            if(var.empty()) continue;
            privSet.insert(var);

            std::string rewritten = rewrite_private_ident(ctx, var);
            if(!outStmt.empty()) outStmt += ",";
            outStmt += rewritten;
        }
        res.outLine = outStmt + ";\n";
        return res;
    }
    // ========== #inline varDecl; ==========
    if(cmd == "inline")
    {
        if(ctx.skipOutput) return res;
        std::string rest;
        std::getline(iss, rest);
        std::string trimmed = ltrim(rest);
        if(!trimmed.empty() && trimmed.back() == ';') trimmed.pop_back();
        trimmed = ltrim(trimmed);

        // 按逗号分割多个声明
        size_t pos = 0;
        while(pos < trimmed.size())
        {
            size_t commaPos = trimmed.find(',', pos);
            std::string item;
            if(commaPos == std::string::npos)
            {
                item = trimmed.substr(pos);
                pos = trimmed.size();
            }
            else
            {
                item = trimmed.substr(pos, commaPos-pos);
                pos = commaPos + 1;
            }
            item = ltrim(item);
            if(item.empty()) continue;

            size_t eqPos = item.find('=');
            if(eqPos != std::string::npos)
            {
                // #inline name=text
                std::string name = ltrim(item.substr(0, eqPos));
                std::string valText = ltrim(item.substr(eqPos+1));
                // 注册到inline符号集合（供 #ifdef 判断）
                ctx.inlineSymbols.insert(name);
                // 自动生成 #define name valText
                DefineMacro dm;
                dm.name = name;
                dm.text = valText;
                ctx.defines.push_back(std::move(dm));
            }
            else
            {
                // old
                ctx.inlineSymbols.insert(item);
            }
        }
        return res;
    }
    // ========== #define ident … #enddef ==========
    if(cmd == "define")
    {
        if(ctx.skipOutput) return res;
        std::string name;
        iss >> name;
        std::string textPart;
        std::getline(iss, textPart);
        DefineMacro m;
        m.name = name;
        m.text = textPart;
        ctx.defines.push_back(m);
        res.consumeRestOfInput = true;
        return res;
    }
    if(cmd == "enddef")
    {
        return res;
    }
    // ========== #ifdef ==========
    if(cmd == "ifdef")
    {
        std::string sym;
        iss >> sym;
        CondFrame fr;
        fr.inIfDef = true;
        fr.anyTaken = false;
        bool cond = ctx.isDefined(sym);
        fr.isActive = !ctx.skipOutput && cond;
        if(fr.isActive) fr.anyTaken = true;
        ctx.condStack.push(fr);
        ctx.skipOutput = !fr.isActive;
        return res;
    }
    if(cmd == "ifndef")
    {
        std::string sym;
        iss >> sym;
        CondFrame fr;
        fr.inIfDef = true;
        fr.anyTaken = false;
        bool cond = !ctx.isDefined(sym);
        fr.isActive = !ctx.skipOutput && cond;
        if(fr.isActive) fr.anyTaken = true;
        ctx.condStack.push(fr);
        ctx.skipOutput = !fr.isActive;
        return res;
    }
    if(cmd == "elif")
    {
        if(ctx.condStack.empty()) return res;
        CondFrame& top = ctx.condStack.top();
        std::string sym;
        iss >> sym;
        if(top.anyTaken)
        {
            top.isActive = false;
            ctx.skipOutput = true;
        }
        else
        {
            bool cond = ctx.isDefined(sym);
            top.isActive = cond;
            if(cond) top.anyTaken = true;
            ctx.skipOutput = !top.isActive;
        }
        return res;
    }
    if(cmd == "else")
    {
        if(ctx.condStack.empty()) return res;
        CondFrame& top = ctx.condStack.top();
        if(top.anyTaken)
        {
            top.isActive = false;
            ctx.skipOutput = true;
        }
        else
        {
            top.isActive = true;
            top.anyTaken = true;
            ctx.skipOutput = false;
        }
        return res;
    }
    if(cmd == "endif")
    {
        if(!ctx.condStack.empty())
        {
            ctx.condStack.pop();
        }
        ctx.skipOutput = false;
        if(!ctx.condStack.empty())
        {
            ctx.skipOutput = !ctx.condStack.top().isActive;
        }
        return res;
    }
    return res;
}

void preproc_include_file(PreProcContext& ctx, const fs::path& baseDir, const std::string& filename, std::string& outSource)
{
    fs::path full = baseDir / filename;
    auto absPath = fs::absolute(full).lexically_normal();
    std::string key = absPath.string();
    if(ctx.included.count(key)) return;
    ctx.included.insert(key);
    std::ifstream fin(absPath);
    if(!fin.is_open())
    {
        std::cerr << "[Preproc] cannot include file: " << absPath << "\n";
        return;
    }
    std::stringstream buf;
    buf << fin.rdbuf();
    fin.close();
    std::string fileContent = buf.str();
    PreProcContext subCtx;
    subCtx.exeDir = ctx.exeDir;
    subCtx.included = ctx.included;       
    subCtx.defines = ctx.defines;         
    subCtx.inlineSymbols = ctx.inlineSymbols;
    subCtx.skipOutput = false;

    std::istringstream iss(fileContent);
    std::string line;
    bool collectDefine = false;
    DefineMacro* curDef = nullptr;
    while(std::getline(iss, line))
    {
        if(collectDefine)
        {
            std::string hb = strip_hash_directive(line);
            if(starts_with(ltrim(hb),"enddef"))
            {
                collectDefine = false;
                curDef = nullptr;
                continue;
            }
            curDef->text += line + "\n";
            continue;
        }
        auto lr = preproc_handle_line(line, subCtx);
        if(lr.consumeRestOfInput)
        {
            collectDefine = true;
            curDef = &subCtx.defines.back();
        }
        outSource += lr.outLine;
    }
    ctx.defines.swap(subCtx.defines);
    ctx.inlineSymbols.swap(subCtx.inlineSymbols);
}


static bool is_id_start(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || static_cast<unsigned char>(c) > 0x7F;
}
static bool is_id_cont(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || static_cast<unsigned char>(c) > 0x7F;
}

std::string rewrite_private_in_line(PreProcContext& ctx, const std::string& line)
{
    if(ctx.classPrivateSetStack.empty())
        return line;
    const auto& privSet = ctx.classPrivateSetStack.top();
    if(privSet.empty())
        return line;

    std::string out;
    size_t i = 0;
    const size_t n = line.size();
    bool in_string = false;
    char string_quote = 0;
    bool in_line_comment = false;

    while(i < n)
    {
        char ch = line[i];
        if(in_line_comment)
        {
            out.push_back(ch);
            i++;
            continue;
        }
        if(in_string)
        {
            // 字符串内，处理转义
            if(ch == '\\' && i+1 < n)
            {
                out.push_back(ch);
                out.push_back(line[i+1]);
                i +=2;
                continue;
            }
            if(ch == string_quote)
            {
                in_string = false;
            }
            out.push_back(ch);
            i++;
            continue;
        }
        // 检测字符串开始
        if(ch == '"')
        {
            in_string = true;
            string_quote = '"';
            out.push_back(ch);
            i++;
            continue;
        }
        // 检测 // 注释
        if(ch == '/' && i+1 < n && line[i+1] == '/')
        {
            in_line_comment = true;
            out.push_back(ch);
            i++;
            continue;
        }
        // 识别标识符
        if(is_id_start(static_cast<unsigned char>(ch)))
        {
            size_t start = i;
            while(i < n && is_id_cont(static_cast<unsigned char>(line[i])))
            {
                i++;
            }
            std::string ident = line.substr(start, i - start);
            if(privSet.count(ident))
            {
                // 需要重写私有字段
                out += rewrite_private_ident(ctx, ident);
            }
            else
            {
                out += ident;
            }
            continue;
        }
        // 普通字符直接复制
        out.push_back(ch);
        i++;
    }
    return out;
}

std::string preprocess_source(const std::string& rawSource, const fs::path& exePath)
{
    PreProcContext ctx;
    ctx.exeDir = exePath.parent_path();
    ctx.skipOutput = false;

    std::istringstream iss(rawSource);
    std::string out;
    std::string line;
    bool collectDefine = false;
    DefineMacro* curDef = nullptr;

    while(std::getline(iss, line))
    {
        std::string lt = ltrim(line);
        if(starts_with(lt,"class "))
        {
            size_t sp = lt.find(' ');
            size_t bpos = lt.find("begin");
            std::string clsName = lt.substr(sp+1, bpos - sp -1);
            clsName = ltrim(clsName);
            ctx.classStack.push(clsName);
            ctx.classPrivateSetStack.push(std::unordered_set<std::string>{}); 
        }
        if(lt == "end")
        {
            if(!ctx.classStack.empty())
            {       
                ctx.classStack.pop();
                ctx.classPrivateSetStack.pop(); 
            }
        }
        if(collectDefine)
        {
            std::string hb = strip_hash_directive(line);
            std::string t = ltrim(hb);
            if(starts_with(t, "enddef"))
            {
                collectDefine = false;
                curDef = nullptr;
                continue;
            }
            curDef->text += line + "\n";
            continue;
        }

        auto lr = preproc_handle_line(line, ctx);
        if(lr.consumeRestOfInput)
        {
            collectDefine = true;
            curDef = &ctx.defines.back();
        }
        out += lr.outLine;
    }
    return out;
}
