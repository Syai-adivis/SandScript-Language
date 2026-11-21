#ifndef ANALYZER_H
#define ANALYZER_H
#include "RuntimeValue.h"
// ===================== TokenType =====================
enum SandTokenType
{
    T_PROPERTY, T_EOF, NUM, STR, IDENT, T_IF, T_ELSE, T_WHILE, T_FOR, T_IN,
    T_FUNC, T_RETURN, T_BREAK, T_CONTINUE, T_PRINT, T_INPUT, T_RANGE,
    T_FROM, T_IMPORT, T_CLASS, T_STATIC, T_NEW, PLUS, MINUS, STAR, SLASH,
    EQ, NEQ, LT, GT, LE, GE, AND_AND, OR_OR, BANG, PLUS_PLUS, MINUS_MINUS,
    PLUS_EQ, MINUS_EQ, STAR_EQ, SLASH_EQ, T_BEGIN, T_END, ASSIGN,
    LPAREN, RPAREN, LBRACK, RBRACK, LBRACE, RBRACE, COMMA, COLON, SEMI,
    T_SUPER, DOTDOTDOT, T_LAMBDA, T_SWITCH, T_CASE, T_DEFAULT, T_NAMESPACE,
    CONNECT, DISCONNECT, T_EMIT, DOT, T_WIDGET, T_SLOT, T_WORKER, T_EXPORT,
    T_DECLARE, T_FORWARD
};
// ===================== ASTNode =====================
class ASTNode
{
public:
    enum Kind
    {
        PROGRAM,
        ASSIGN,
        INDEX_ASSIGN,
        BINARY,
        LIT_NUM,
        LIT_STR,
        VAR,
        IF,
        WHILE,
        FOR_IN,
        CALL,
        FUNC_DEF,
        RETURN,
        RANGE,
        ARRAY_LIT,
        INDEX,
        DICT_LIT,
        BREAK,
        CONTINUE,
        UNARY,
        COMPOUND_ASSIGN,
        INCDEC,
        IMPORT,
        CLASS_DEF,
        NEW_OBJ,
        MEMBER_ACCESS,
        MEMBER_CALL,
        MEMBER_ASSIGN,
        SWITCH,
        CASE_PATTERN,
        NAMESPACE_DEF,
        PARAM_DEF,
        SLOT_CONNECT_EXPR,
        SLOT_DISCONNECT_EXPR,
        EMIT_EXPR,
        LAMBDA_EXPR,
        WIDGET_DECL,
        WIDGET_PROP,
        WIDGET_CLASS_DEF,
        WIDGET_EXPORT_PROP,
        WIDGET_EXPORT_WIDGET,
        WIDGET_EXPORT_SIGNAL,
        FORWARD_DECL
    } kind;
    std::vector<std::unique_ptr<ASTNode>> children;
    std::string val;
    std::string val2;
    RuntimeVal literal;
    size_t line = 0;
    ASTNode(Kind k) : kind(k) {}
};

struct Token
{
    SandTokenType type;
    std::string val;
    size_t pos{};
    size_t line = 1;
};
// ===================== Lexer =====================
struct Lexer
{
    std::string src;
    size_t idx = 0;
    size_t cur_line = 1;
    char peek() { return idx >= src.size() ? 0 : src[idx]; }
    char consume()
    {
        char c = src[idx++];
        if (c == '\n')
            cur_line++;
        return c;
    }
    void skip_ws()
    {
        while (true)
        {
            unsigned char ch = static_cast<unsigned char>(peek());
            while (std::isspace(ch))
            {
                consume();
                ch = static_cast<unsigned char>(peek());
            }
            if (peek() == '/' && idx + 1 < src.size() && src[idx + 1] == '/')
            {
                consume();
                consume();
                while (peek() != 0 && peek() != '\n')
                    consume();
            }
            else
                break;
        }
    }
    Token read_str()
    {
        size_t ln = cur_line;
        consume();
        std::string out;
        while (peek() != '"' && peek() != 0)
        {
            char c = consume();
            if (c == '\\')
            {
                char esc = peek();
                if (esc == 0)
                    break;
                consume();
                if (esc == 'n')
                    out += '\n';
                else if (esc == '\\')
                    out += '\\';
                else if (esc == '"')
                    out += '"';
                else
                    out += esc;
            }
            else
                out += c;
        }
        if (peek() == '"')
            consume();
        return Token{STR, out, idx, ln};
    }
    Token read_ident()
    {
        size_t start = idx;
        size_t ln = cur_line;
        for (;;)
        {
            unsigned char ch = static_cast<unsigned char>(peek());
            bool ok = std::isalnum(ch) || peek() == '_' || peek() > 0x7F;
            if (!ok)
                break;
            consume();
        }
        std::string word = src.substr(start, idx - start);
        SandTokenType t = IDENT;
        if (word == "if")
            t = T_IF;
        else if (word == "else")
            t = T_ELSE;
        else if (word == "while")
            t = T_WHILE;
        else if (word == "for")
            t = T_FOR;
        else if (word == "and")
            t = AND_AND;
        else if (word == "or")
            t = OR_OR;
        else if (word == "not")
            t = BANG;
        else if (word == "in")
            t = T_IN;
        else if (word == "func")
            t = T_FUNC;
        else if (word == "return")
            t = T_RETURN;
        else if (word == "break")
            t = T_BREAK;
        else if (word == "continue")
            t = T_CONTINUE;
        else if (word == "output")
            t = T_PRINT;
        else if (word == "input")
            t = T_INPUT;
        else if (word == "range")
            t = T_RANGE;
        else if (word == "begin")
            t = T_BEGIN;
        else if (word == "end")
            t = T_END;
        else if (word == "from")
            t = T_FROM;
        else if (word == "import")
            t = T_IMPORT;
        else if (word == "class")
            t = T_CLASS;
        else if (word == "static")
            t = T_STATIC;
        else if (word == "new")
            t = T_NEW;
        else if (word == "super")
            t = T_SUPER;
        else if (word == "lambda")
            t = T_LAMBDA;
        else if (word == "switch")
            t = T_SWITCH;
        else if (word == "case")
            t = T_CASE;
        else if (word == "default")
            t = T_DEFAULT;
        else if (word == "zhumipingan")
            t = T_DEFAULT;
        else if (word == "namespace")
            t = T_NAMESPACE;
        else if (word == "emit")
            t = T_EMIT;
        else if (word == "widget")
            t = T_WIDGET;
        else if (word == "property")
            t = T_PROPERTY;
        else if (word == "slot")
            t = T_SLOT;
        else if (word == "worker")
            t = T_WORKER;
        else if (word == "export")
            t = T_EXPORT;
        else if (word == "declare")
            t = T_DECLARE;
        else if (word == "forward")
            t = T_FORWARD;
        else if (word == "true")
            return Token{NUM, "1", start, ln};
        else if (word == "false")
            return Token{NUM, "0", start, ln};
        return Token{t, word, start, ln};
    }
    Token read_num()
    {
        size_t start = idx;
        size_t ln = cur_line;
        while (std::isdigit(static_cast<unsigned char>(peek())))
            consume();
        if (peek() == '.' && idx + 1 < src.size() &&
            std::isdigit(static_cast<unsigned char>(src[idx + 1])))
        {
            consume();
            while (std::isdigit(static_cast<unsigned char>(peek())))
                consume();
        }
        return Token{NUM, src.substr(start, idx - start), start, ln};
    }
    Token next()
    {
        skip_ws();
        char rc = peek();
        size_t ln = cur_line;
        unsigned char c = static_cast<unsigned char>(rc);
        if (!c)
            return Token{T_EOF, "", idx, ln};
        if (isalpha(c) || c > 0x7F)
            return read_ident();
        if (isdigit(c))
            return read_num();
        if (c == '"')
            return read_str();
        if (c == '.')
        {
            if (idx + 2 < src.size() && src[idx + 1] == '.' && src[idx + 2] == '.')
            {
                consume();
                consume();
                consume();
                return Token{DOTDOTDOT, "...", idx - 3, ln};
            }
            consume();
            return Token{DOT, ".", idx - 1, ln};
        }
        if (c == '(')
        {
            consume();
            return Token{LPAREN, "(", idx - 1, ln};
        }
        if (c == ')')
        {
            consume();
            return Token{RPAREN, ")", idx - 1, ln};
        }
        if (c == '[')
        {
            consume();
            return Token{LBRACK, "[", idx - 1, ln};
        }
        if (c == ']')
        {
            consume();
            return Token{RBRACK, "]", idx - 1, ln};
        }
        if (c == '{')
        {
            consume();
            return Token{LBRACE, "{", idx - 1, ln};
        }
        if (c == '}')
        {
            consume();
            return Token{RBRACE, "}", idx - 1, ln};
        }
        if (c == ',')
        {
            consume();
            return Token{COMMA, ",", idx - 1, ln};
        }
        if (c == ':')
        {
            consume();
            return Token{COLON, ":", idx - 1, ln};
        }
        if (c == ';')
        {
            consume();
            return Token{SEMI, ";", idx - 1, ln};
        }
        if (c == '+')
        {
            consume();
            if (peek() == '+')
            {
                consume();
                return Token{PLUS_PLUS, "++", idx - 2, ln};
            }
            if (peek() == '=')
            {
                consume();
                return Token{PLUS_EQ, "+=", idx - 2, ln};
            }
            return Token{PLUS, "+", idx - 1, ln};
        }
        if (c == '-')
        {
            consume();
            if (peek() == '-')
            {
                consume();
                return Token{MINUS_MINUS, "--", idx - 2, ln};
            }
            if (peek() == '=')
            {
                consume();
                return Token{MINUS_EQ, "-=", idx - 2, ln};
            }
            return Token{MINUS, "-", idx - 1, ln};
        }
        if (c == '*')
        {
            consume();
            if (peek() == '=')
            {
                consume();
                return Token{STAR_EQ, "*=", idx - 2, ln};
            }
            return Token{STAR, "*", idx - 1, ln};
        }
        if (c == '/')
        {
            consume();
            if (peek() == '=')
            {
                consume();
                return Token{SLASH_EQ, "/=", idx - 2, ln};
            }
            return Token{SLASH, "/", idx - 1, ln};
        }
        if (c == '=')
        {
            consume();
            if (peek() == '=')
            {
                consume();
                return Token{EQ, "==", idx - 2, ln};
            }
            return Token{ASSIGN, "=", idx - 1, ln};
        }
        if (c == '!')
        {
            consume();
            if (peek() == '=')
            {
                consume();
                return Token{NEQ, "!=", idx - 2, ln};
            }
            if (peek() == '>')
            {
                consume();
                return Token{DISCONNECT, "!>", idx - 2, ln};
            }
            return Token{BANG, "!", idx - 1, ln};
        }
        if (c == '<')
        {
            consume();
            if (peek() == '=')
            {
                consume();
                return Token{LE, "<=", idx - 2, ln};
            }
            return Token{LT, "<", idx - 1, ln};
        }
        if (c == '>')
        {
            consume();
            if (peek() == '=')
            {
                consume();
                return Token{GE, ">=", idx - 2, ln};
            }
            if (peek() == '>')
            {
                consume();
                return Token{CONNECT, ">>", idx - 2, ln};
            }
            return Token{GT, ">", idx - 1, ln};
        }
        if (c == '&')
        {
            consume();
            if (peek() == '&')
            {
                consume();
                return Token{AND_AND, "&&", idx - 2, ln};
            }
        }
        if (c == '|')
        {
            consume();
            if (peek() == '|')
            {
                consume();
                return Token{OR_OR, "||", idx - 2, ln};
            }
        }
        Console::setColor(Console::Color::Red);
        std::cerr << "[" << ln << "] Lexer error: unknown character '" << c << "'\n";
        Console::setColor(Console::Color::Reset);
        consume();
        return next();
    }
};
// ===================== Scope =====================
struct Scope
{
    std::unordered_map<std::string, RuntimeVal> vars;
    Scope *parent = nullptr;
    std::shared_ptr<void> lifetime = std::make_shared<int>(0);
    bool isolateWrites = false;
    Scope(Scope *p = nullptr) : parent(p) {}
    RuntimeVal *get(const std::string &name)
    {
        auto it = vars.find(name);
        if (it != vars.end())
            return &it->second;
        if (parent)
            return parent->get(name);
        return nullptr;
    }
    void set(const std::string &name, RuntimeVal v)
    {
        if (isolateWrites)
        {
            vars[name] = std::move(v);
            return;
        }
        auto p = get(name);
        if (p != nullptr)
        {
            *p = std::move(v);
            return;
        }
        vars[name] = std::move(v);
    }
};
struct Interpreter;
std::unique_ptr<ASTNode> parse_source(const std::string &src);
// ===================== Parser =====================
struct Parser
{
    Lexer lex;
    Token tok;
    size_t last_token_line = 0;
    bool has_token = false;
    void next_tok()
    {
        if (has_token)
            last_token_line = tok.line;
        tok = lex.next();
        has_token = true;
    }
    bool expect(SandTokenType t)
    {
        if (tok.type == t)
        {
            next_tok();
            return true;
        }
        if (t == SEMI &&
            (tok.type == T_EOF || tok.type == T_END || tok.type == RBRACE ||
             tok.line > last_token_line))
            return true;
        Console::setColor(Console::Color::Red);
        std::cerr << "[" << tok.line << "] Syntax error: unexpected token '" << tok.val << "'\n";
        Console::setColor(Console::Color::Reset);
        return false;
    }
    std::unique_ptr<ASTNode> parse_program();
    std::unique_ptr<ASTNode> parse_stmt();
    std::unique_ptr<ASTNode> parse_if();
    std::unique_ptr<ASTNode> parse_while();
    std::unique_ptr<ASTNode> parse_for_in();
    std::unique_ptr<ASTNode> parse_func(bool is_worker = false);
    std::unique_ptr<ASTNode> parse_block();
    std::unique_ptr<ASTNode> parse_expr();
    std::unique_ptr<ASTNode> parse_logic_or();
    std::unique_ptr<ASTNode> parse_logic_and();
    std::unique_ptr<ASTNode> parse_range();
    std::unique_ptr<ASTNode> parse_compare();
    std::unique_ptr<ASTNode> parse_add();
    std::unique_ptr<ASTNode> parse_mul();
    std::unique_ptr<ASTNode> parse_unary();
    std::unique_ptr<ASTNode> parse_primary();
    std::unique_ptr<ASTNode> parse_call_or_index(const std::string &name, size_t ln);
    std::unique_ptr<ASTNode> parse_class();
    std::unique_ptr<ASTNode> parse_new();
    std::unique_ptr<ASTNode> parse_member(std::unique_ptr<ASTNode> base, size_t ln);
    std::unique_ptr<ASTNode> parse_lambda();
    std::unique_ptr<ASTNode> parse_case_pattern_expr();
    std::unique_ptr<ASTNode> parse_switch();
    std::unique_ptr<ASTNode> parse_namespace();
    std::unique_ptr<ASTNode> parse_slot_op(std::unique_ptr<ASTNode> lhs, size_t ln);
    std::unique_ptr<ASTNode> parse_widget(std::unique_ptr<ASTNode> lhs, size_t ln);
    std::unique_ptr<ASTNode> parse_widget_prop(std::unique_ptr<ASTNode> lhs, size_t ln);
    std::unique_ptr<ASTNode> parse_widget_class();
    std::unique_ptr<ASTNode> parse_forward_decl();
    static SandTokenType block_end(SandTokenType open)
    {
        return open == LBRACE ? RBRACE : T_END;
    }
};
#endif