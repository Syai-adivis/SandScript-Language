#include "RuntimeValue.h"
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
        LAMBDA_EXPR
    } kind;
    std::vector<std::unique_ptr<ASTNode>> children;
    std::string val;
    std::string val2;
    RuntimeVal literal;
    size_t line = 0;
    ASTNode(Kind k) : kind(k) {}
};
// ===================== TokenType =====================
enum TokenType
{
    T_EOF,
    NUM,
    STR,
    IDENT,
    T_IF,
    T_ELSE,
    T_WHILE,
    T_FOR,
    T_IN,
    T_FUNC,
    T_RETURN,
    T_BREAK,
    T_CONTINUE,
    T_PRINT,
    T_INPUT,
    T_RANGE,
    T_FROM,
    T_IMPORT,
    T_CLASS,
    T_STATIC,
    T_NEW,
    PLUS,
    MINUS,
    STAR,
    SLASH,
    EQ,
    NEQ,
    LT,
    GT,
    LE,
    GE,
    AND_AND,
    OR_OR,
    BANG,
    PLUS_PLUS,
    MINUS_MINUS,
    PLUS_EQ,
    MINUS_EQ,
    STAR_EQ,
    SLASH_EQ,
    T_BEGIN,
    T_END,
    ASSIGN,
    LPAREN,
    RPAREN,
    LBRACK,
    RBRACK,
    LBRACE,
    RBRACE,
    COMMA,
    COLON,
    SEMI,
    T_SUPER,
    DOTDOTDOT,
    T_LAMBDA,
    T_SWITCH,
    T_CASE,
    T_DEFAULT,
    DOT
};
struct Token
{
    TokenType type;
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
        TokenType t = IDENT;
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
        return Token{t, word, start, ln};
    }
    Token read_num()
    {
        size_t start = idx;
        size_t ln = cur_line;
        while (isdigit(peek()) || peek() == '.')
            consume();
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
        std::cerr << "[" << ln << "] Lexer error: unknown character '" << c << "'\n";
        consume();
        return next();
    }
};
// ===================== Scope =====================
struct Scope
{
    std::unordered_map<std::string, RuntimeVal> vars;
    Scope *parent = nullptr;
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
    void next_tok() { tok = lex.next(); }
    bool expect(TokenType t)
    {
        if (tok.type == t)
        {
            next_tok();
            return true;
        }
        std::cerr << "[" << tok.line << "] Syntax error: unexpected token '" << tok.val << "'\n";
        return false;
    }
    std::unique_ptr<ASTNode> parse_program();
    std::unique_ptr<ASTNode> parse_stmt();
    std::unique_ptr<ASTNode> parse_if();
    std::unique_ptr<ASTNode> parse_while();
    std::unique_ptr<ASTNode> parse_for_in();
    std::unique_ptr<ASTNode> parse_func();
    std::unique_ptr<ASTNode> parse_block();
    std::unique_ptr<ASTNode> parse_expr();
    std::unique_ptr<ASTNode> parse_logic_or();
    std::unique_ptr<ASTNode> parse_logic_and();
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
};
std::unique_ptr<ASTNode> Parser::parse_program()
{
    auto prog = std::make_unique<ASTNode>(ASTNode::PROGRAM);
    prog->line = lex.cur_line;
    while (tok.type != T_EOF)
    {
        prog->children.push_back(parse_stmt());
    }
    return prog;
}
std::unique_ptr<ASTNode> Parser::parse_stmt()
{
    size_t ln = tok.line;
    switch (tok.type)
    {
    case T_IF:
        return parse_if();
    case T_WHILE:
        return parse_while();
    case T_FOR:
        return parse_for_in();
    case T_FUNC:
        return parse_func();
    case T_CLASS:
        return parse_class();
    // case T_NEW:
    // return parse_new();
    case T_FROM:
    {
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::IMPORT);
        n->line = ln;
        if (tok.type != STR)
        {
            std::cerr << "[" << tok.line << "] Syntax error: expect string filename for from...import\n";
            return std::make_unique<ASTNode>(ASTNode::PROGRAM);
        }
        n->val = tok.val;
        next_tok();
        expect(T_IMPORT);
        expect(SEMI);
        return n;
    }
    case T_RETURN:
    {
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::RETURN);
        n->line = ln;
        n->children.push_back(parse_expr());
        expect(SEMI);
        return n;
    }
    case T_BREAK:
    {
        next_tok();
        expect(SEMI);
        auto n = std::make_unique<ASTNode>(ASTNode::BREAK);
        n->line = ln;
        return n;
    }
    case T_CONTINUE:
    {
        next_tok();
        expect(SEMI);
        auto n = std::make_unique<ASTNode>(ASTNode::CONTINUE);
        n->line = ln;
        return n;
    }
    case T_PRINT:
    {
        next_tok();
        expect(LPAREN);
        auto n = std::make_unique<ASTNode>(ASTNode::CALL);
        n->val = "output";
        n->line = ln;
        n->children.push_back(parse_expr());
        expect(RPAREN);
        expect(SEMI);
        return n;
    }
    case T_INPUT:
    {
        next_tok();
        expect(LPAREN);
        expect(RPAREN);
        auto n = std::make_unique<ASTNode>(ASTNode::CALL);
        n->val = "input";
        n->line = ln;
        expect(SEMI);
        return n;
    }
    case IDENT:
    {
        std::string name = tok.val;
        size_t l = tok.line;
        next_tok();
        if (tok.type == PLUS_PLUS || tok.type == MINUS_MINUS)
        {
            TokenType op = tok.type;
            next_tok();
            auto n = std::make_unique<ASTNode>(ASTNode::INCDEC);
            n->val = (op == PLUS_PLUS) ? name + "++" : name + "--";
            n->line = l;
            expect(SEMI);
            return n;
        }
        if (tok.type == ASSIGN)
        {
            next_tok();
            auto n = std::make_unique<ASTNode>(ASTNode::ASSIGN);
            n->val = name;
            n->line = l;
            n->children.push_back(parse_expr());
            expect(SEMI);
            return n;
        }
        if (tok.type == PLUS_EQ || tok.type == MINUS_EQ || tok.type == STAR_EQ || tok.type == SLASH_EQ)
        {
            std::string opstr = tok.val;
            next_tok();
            auto n = std::make_unique<ASTNode>(ASTNode::COMPOUND_ASSIGN);
            n->val = name + ":" + opstr;
            n->line = l;
            n->children.push_back(parse_expr());
            expect(SEMI);
            return n;
        }
        else if (tok.type == LBRACK)
        {
            next_tok();
            auto idx_expr = parse_expr();
            expect(RBRACK);
            if (tok.type == PLUS_PLUS || tok.type == MINUS_MINUS)
            {
                TokenType op = tok.type;
                next_tok();
                auto n = std::make_unique<ASTNode>(ASTNode::INCDEC);
                n->val = (op == PLUS_PLUS) ? "INDEX:++" : "INDEX:--";
                n->line = l;
                n->children.push_back(std::move(idx_expr));
                auto base = std::make_unique<ASTNode>(ASTNode::VAR);
                base->val = name;
                base->line = l;
                n->children.push_back(std::move(base));
                expect(SEMI);
                return n;
            }
            if (tok.type == ASSIGN)
            {
                next_tok();
                auto node = std::make_unique<ASTNode>(ASTNode::INDEX_ASSIGN);
                node->val = name;
                node->line = l;
                node->children.push_back(std::move(idx_expr));
                node->children.push_back(parse_expr());
                expect(SEMI);
                return node;
            }
            if (tok.type == PLUS_EQ || tok.type == MINUS_EQ || tok.type == STAR_EQ || tok.type == SLASH_EQ)
            {
                std::string opstr = tok.val;
                next_tok();
                auto n = std::make_unique<ASTNode>(ASTNode::COMPOUND_ASSIGN);
                n->val = "INDEX:" + opstr;
                n->line = l;
                n->children.push_back(std::move(idx_expr));
                n->children.push_back(parse_expr());
                auto base = std::make_unique<ASTNode>(ASTNode::VAR);
                base->val = name;
                base->line = l;
                n->children.push_back(std::move(base));
                expect(SEMI);
                return n;
            }
            else
            {
                auto idxnode = std::make_unique<ASTNode>(ASTNode::INDEX);
                idxnode->val = name;
                idxnode->line = l;
                idxnode->children.push_back(std::move(idx_expr));
                auto wrap = std::make_unique<ASTNode>(ASTNode::ASSIGN);
                wrap->val = "_expr";
                wrap->children.push_back(std::move(idxnode));
                expect(SEMI);
                return wrap;
            }
        }
        else
        {
            auto e = parse_call_or_index(name, l);
            if (e->kind == ASTNode::MEMBER_ACCESS && tok.type == ASSIGN)
            {
                next_tok();
                auto mas = std::make_unique<ASTNode>(ASTNode::MEMBER_ASSIGN);
                mas->val = e->val;
                mas->line = e->line;
                mas->children.push_back(std::move(e->children[0]));
                mas->children.push_back(parse_expr());
                expect(SEMI);
                return mas;
            }
            expect(SEMI);
            auto wrap = std::make_unique<ASTNode>(ASTNode::ASSIGN);
            wrap->val = "_expr";
            wrap->children.push_back(std::move(e));
            return wrap;
        }
    }
    default:
    {
        auto e = parse_expr();
        expect(SEMI);
        auto wrap = std::make_unique<ASTNode>(ASTNode::ASSIGN);
        wrap->val = "_expr";
        wrap->children.push_back(std::move(e));
        return wrap;
    }
    }
}
std::unique_ptr<ASTNode> Parser::parse_case_pattern_expr()
{
    using ParseFn = std::function<std::unique_ptr<ASTNode>()>;
    ParseFn parse_pattern_or;
    parse_pattern_or = [&]() -> std::unique_ptr<ASTNode>
    {
        auto lhs = [&]() -> std::unique_ptr<ASTNode>
        {
            auto lhs = parse_primary();
            while (tok.type == AND_AND)
            {
                size_t ln = tok.line;
                std::string op = tok.val;
                next_tok();
                auto n = std::make_unique<ASTNode>(ASTNode::BINARY);
                n->val = op;
                n->line = ln;
                n->children.push_back(std::move(lhs));
                n->children.push_back(parse_primary());
                lhs = std::move(n);
            }
            return lhs;
        }();
        while (tok.type == OR_OR)
        {
            size_t ln = tok.line;
            std::string op = tok.val;
            next_tok();
            auto n = std::make_unique<ASTNode>(ASTNode::BINARY);
            n->val = op;
            n->line = ln;
            n->children.push_back(std::move(lhs));
            n->children.push_back(parse_pattern_or());
            lhs = std::move(n);
        }
        return lhs;
    };
    return parse_pattern_or();
}
std::unique_ptr<ASTNode> Parser::parse_switch()
{
    size_t ln = tok.line;
    next_tok();
    expect(LPAREN);
    auto subject_expr = parse_expr();
    expect(RPAREN);
    expect(T_BEGIN);

    auto switch_node = std::make_unique<ASTNode>(ASTNode::SWITCH);
    switch_node->line = ln;
    switch_node->children.push_back(std::move(subject_expr));

    std::unordered_set<std::string> case_literals;
    bool seen_default = false;

    while (tok.type != T_END && tok.type != T_EOF)
    {
        size_t case_ln = tok.line;
        bool is_default = (tok.type == T_DEFAULT);
        if (is_default)
        {
            next_tok();
            if (seen_default)
            {
                std::cerr << "[" << case_ln << "] Syntax error: multiple default in one switch\n";
            }
            seen_default = true;
        }
        else if (tok.type == T_CASE)
        {
            next_tok();
        }
        else
        {
            std::cerr << "[" << tok.line << "] Syntax error: expect case / default inside switch begin\n";
            break;
        }

        std::unique_ptr<ASTNode> pattern_expr;
        if (!is_default)
        {
            pattern_expr = parse_case_pattern_expr();
            if (pattern_expr->kind == ASTNode::LIT_NUM || pattern_expr->kind == ASTNode::LIT_STR)
            {
                std::string lit_key = pattern_expr->literal.to_string();
                if (case_literals.count(lit_key))
                {
                    std::cerr << "[" << case_ln << "] Syntax error: duplicate case literal " << lit_key << "\n";
                }
                case_literals.insert(lit_key);
            }
        }

        std::unique_ptr<ASTNode> guard_expr = nullptr;
        if (tok.type == T_IF)
        {
            next_tok();
            guard_expr = parse_expr();
        }

        expect(COLON);
        auto body_stmt = parse_stmt();

        auto case_node = std::make_unique<ASTNode>(ASTNode::CASE_PATTERN);
        case_node->val = is_default ? "default" : "case";
        case_node->line = case_ln;
        if (!is_default)
        {
            case_node->children.push_back(std::move(pattern_expr));
        }
        else
        {
            case_node->children.push_back(nullptr);
        }
        if (guard_expr)
        {
            case_node->children.push_back(std::move(guard_expr));
        }
        else
        {
            auto true_lit = std::make_unique<ASTNode>(ASTNode::LIT_NUM);
            true_lit->literal = RuntimeVal(BigDecimal("1"));
            case_node->children.push_back(std::move(true_lit));
        }
        case_node->children.push_back(std::move(body_stmt));

        switch_node->children.push_back(std::move(case_node));
    }
    expect(T_END);
    return switch_node;
}
std::unique_ptr<ASTNode> Parser::parse_lambda()
{
    size_t ln = tok.line;
    next_tok();
    expect(LPAREN);
    std::vector<std::string> params;
    while (tok.type != RPAREN)
    {
        if (tok.type == DOTDOTDOT)
        {
            next_tok();
            if (tok.type != IDENT)
            {
                std::cerr << "[" << tok.line << "] Syntax error: ... requires identifier\n";
            }
            params.push_back("..." + tok.val);
            next_tok();
            break;
        }
        params.push_back(tok.val);
        expect(IDENT);
        if (tok.type == COMMA)
            next_tok();
    }
    expect(RPAREN);
    auto block = parse_block();
    auto check_no_nested_func = [&](auto &&self, ASTNode *node) -> bool
    {
        if (node->kind == ASTNode::FUNC_DEF || node->kind == ASTNode::LAMBDA_EXPR)
        {
            std::cerr << "[" << node->line << "] Syntax error: nested function/lambda is not allowed\n";
            return false;
        }
        for (auto &ch : node->children)
        {
            if (!self(self, ch.get()))
                return false;
        }
        return true;
    };
    if (!check_no_nested_func(check_no_nested_func, block.get()))
    {
        return std::make_unique<ASTNode>(ASTNode::PROGRAM);
    }

    auto node = std::make_unique<ASTNode>(ASTNode::LAMBDA_EXPR);
    node->line = ln;
    for (auto &p : params)
    {
        auto v = std::make_unique<ASTNode>(ASTNode::VAR);
        v->val = p;
        node->children.push_back(std::move(v));
    }
    node->children.push_back(std::move(block));
    return node;
}

std::unique_ptr<ASTNode> Parser::parse_if()
{
    size_t ln = tok.line;
    next_tok();
    expect(LPAREN);
    auto node = std::make_unique<ASTNode>(ASTNode::IF);
    node->line = ln;
    node->children.push_back(parse_expr());
    expect(RPAREN);
    node->children.push_back(parse_block());
    if (tok.type == T_ELSE)
    {
        next_tok();
        if (tok.type == T_IF)
        {
            node->children.push_back(parse_if());
        }
        else
        {
            node->children.push_back(parse_block());
        }
    }
    return node;
}
std::unique_ptr<ASTNode> Parser::parse_while()
{
    size_t ln = tok.line;
    next_tok();
    expect(LPAREN);
    auto node = std::make_unique<ASTNode>(ASTNode::WHILE);
    node->line = ln;
    node->children.push_back(parse_expr());
    expect(RPAREN);
    node->children.push_back(parse_block());
    return node;
}
std::unique_ptr<ASTNode> Parser::parse_for_in()
{
    size_t ln = tok.line;
    next_tok();
    std::string var = tok.val;
    expect(IDENT);
    expect(T_IN);
    auto node = std::make_unique<ASTNode>(ASTNode::FOR_IN);
    node->val = var;
    node->line = ln;
    node->children.push_back(parse_expr());
    node->children.push_back(parse_block());
    return node;
}
std::unique_ptr<ASTNode> Parser::parse_func()
{
    size_t ln = tok.line;
    next_tok();
    std::string name = tok.val;
    expect(IDENT);
    expect(LPAREN);
    std::vector<std::string> params;
    bool variadic = false;
    while (tok.type != RPAREN)
    {
        if (tok.type == DOTDOTDOT)
        {
            next_tok();
            if (tok.type != IDENT)
            {
                std::cerr << "[" << tok.line << "] Syntax error: ... requires identifier\n";
            }
            params.push_back("..." + tok.val);
            variadic = true;
            next_tok();
            break;
        }
        params.push_back(tok.val);
        expect(IDENT);
        if (tok.type == COMMA)
            next_tok();
    }
    expect(RPAREN);
    auto body = parse_block();
    auto node = std::make_unique<ASTNode>(ASTNode::FUNC_DEF);
    node->val = name;
    node->line = ln;
    for (auto &p : params)
    {
        auto vnode = std::make_unique<ASTNode>(ASTNode::VAR);
        vnode->val = p;
        node->children.push_back(std::move(vnode));
    }
    node->children.push_back(std::move(body));
    return node;
}
std::unique_ptr<ASTNode> Parser::parse_block()
{
    if (!expect(T_BEGIN))
        return std::make_unique<ASTNode>(ASTNode::PROGRAM);
    auto blk = std::make_unique<ASTNode>(ASTNode::PROGRAM);
    int depth = 1;
    while (tok.type != T_EOF && depth > 0)
    {
        if (tok.type == T_BEGIN)
            depth++;
        else if (tok.type == T_END)
        {
            depth--;
            if (depth == 0)
                break;
        }
        blk->children.push_back(parse_stmt());
    }
    if (!expect(T_END))
        std::cerr << "[" << tok.line << "] Syntax error: missing 'end' for block\n";
    return blk;
}
std::unique_ptr<ASTNode> Parser::parse_expr() { return parse_logic_or(); }
std::unique_ptr<ASTNode> Parser::parse_logic_or()
{
    auto lhs = parse_logic_and();
    while (tok.type == OR_OR)
    {
        std::string op = tok.val;
        size_t ln = tok.line;
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::BINARY);
        n->val = op;
        n->line = ln;
        n->children.push_back(std::move(lhs));
        n->children.push_back(parse_logic_and());
        lhs = std::move(n);
    }
    return lhs;
}
std::unique_ptr<ASTNode> Parser::parse_logic_and()
{
    auto lhs = parse_compare();
    while (tok.type == AND_AND)
    {
        std::string op = tok.val;
        size_t ln = tok.line;
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::BINARY);
        n->val = op;
        n->line = ln;
        n->children.push_back(std::move(lhs));
        n->children.push_back(parse_compare());
        lhs = std::move(n);
    }
    return lhs;
}
std::unique_ptr<ASTNode> Parser::parse_compare()
{
    auto lhs = parse_add();
    for (;;)
    {
        TokenType op = tok.type;
        if (op != EQ && op != NEQ && op != LT && op != GT && op != LE && op != GE)
            break;
        std::string opstr = tok.val;
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::BINARY);
        n->val = opstr;
        n->line = tok.line;
        n->children.push_back(std::move(lhs));
        n->children.push_back(parse_add());
        lhs = std::move(n);
    }
    return lhs;
}
std::unique_ptr<ASTNode> Parser::parse_add()
{
    auto lhs = parse_mul();
    while (tok.type == PLUS || tok.type == MINUS)
    {
        std::string op = tok.val;
        size_t ln = tok.line;
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::BINARY);
        n->val = op;
        n->line = ln;
        n->children.push_back(std::move(lhs));
        n->children.push_back(parse_mul());
        lhs = std::move(n);
    }
    return lhs;
}
std::unique_ptr<ASTNode> Parser::parse_mul()
{
    auto lhs = parse_unary();
    while (tok.type == STAR || tok.type == SLASH)
    {
        std::string op = tok.val;
        size_t ln = tok.line;
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::BINARY);
        n->val = op;
        n->line = ln;
        n->children.push_back(std::move(lhs));
        n->children.push_back(parse_unary());
        lhs = std::move(n);
    }
    return lhs;
}
std::unique_ptr<ASTNode> Parser::parse_unary()
{
    if (tok.type == MINUS || tok.type == BANG)
    {
        TokenType op = tok.type;
        size_t ln = tok.line;
        std::string opstr = tok.val;
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::UNARY);
        n->val = opstr;
        n->line = ln;
        n->children.push_back(parse_unary());
        return n;
    }
    return parse_primary();
}
std::unique_ptr<ASTNode> Parser::parse_primary()
{
    Token t = tok;
    size_t ln = t.line;
    switch (t.type)
    {
    case NUM:
    {
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::LIT_NUM);
        n->literal = RuntimeVal(BigDecimal(t.val));
        n->line = ln;
        return n;
    }
    case STR:
    {
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::LIT_STR);
        n->literal = RuntimeVal(std::string(t.val));
        n->line = ln;
        return n;
    }
    case LPAREN:
    {
        next_tok();
        auto e = parse_expr();
        expect(RPAREN);
        return e;
    }
    case LBRACK:
    {
        next_tok();
        auto arr = std::make_unique<ASTNode>(ASTNode::ARRAY_LIT);
        arr->line = ln;
        while (tok.type != RBRACK)
        {
            arr->children.push_back(parse_expr());
            if (tok.type == COMMA)
                next_tok();
        }
        expect(RBRACK);
        return arr;
    }
    case LBRACE:
    {
        next_tok();
        auto dict = std::make_unique<ASTNode>(ASTNode::DICT_LIT);
        dict->line = ln;
        while (tok.type != RBRACE)
        {
            dict->children.push_back(parse_expr());
            expect(COLON);
            dict->children.push_back(parse_expr());
            if (tok.type == COMMA)
                next_tok();
        }
        expect(RBRACE);
        return dict;
    }
    case T_INPUT:
    {
        next_tok();
        expect(LPAREN);
        expect(RPAREN);
        auto n = std::make_unique<ASTNode>(ASTNode::CALL);
        n->val = "input";
        n->line = ln;
        return n;
    }
    case T_RANGE:
    {
        next_tok();
        expect(LPAREN);
        auto n = std::make_unique<ASTNode>(ASTNode::RANGE);
        n->line = ln;
        n->children.push_back(parse_expr());
        expect(COMMA);
        n->children.push_back(parse_expr());
        if (tok.type == COMMA)
        {
            next_tok();
            n->children.push_back(parse_expr());
        }
        expect(RPAREN);
        return n;
    }
    case IDENT:
    {
        std::string name = tok.val;
        next_tok();
        return parse_call_or_index(name, ln);
    }
    case T_NEW:
    {
        return parse_new();
    }
    case T_SUPER:
    {
        next_tok();
        return parse_call_or_index("super", ln);
    }
    case T_LAMBDA:
    {
        return parse_lambda();
    }
    default:
        std::cerr << "[" << ln << "] Parse error: bad expression\n";
        next_tok();
        return std::make_unique<ASTNode>(ASTNode::LIT_NUM);
    }
}
std::unique_ptr<ASTNode> Parser::parse_call_or_index(const std::string &name, size_t ln)
{
    std::unique_ptr<ASTNode> node;
    if (tok.type == LPAREN)
    {
        next_tok();
        auto call = std::make_unique<ASTNode>(ASTNode::CALL);
        call->val = name;
        call->line = ln;
        while (tok.type != RPAREN)
        {
            call->children.push_back(parse_expr());
            if (tok.type == COMMA)
                next_tok();
        }
        expect(RPAREN);
        node = std::move(call);
    }
    else if (tok.type == LBRACK)
    {
        next_tok();
        auto idx = std::make_unique<ASTNode>(ASTNode::INDEX);
        idx->val = name;
        idx->line = ln;
        idx->children.push_back(parse_expr());
        expect(RBRACK);
        node = std::move(idx);
    }
    else
    {
        auto v = std::make_unique<ASTNode>(ASTNode::VAR);
        v->val = name;
        v->line = ln;
        node = std::move(v);
    }
    // 链式成员访问 a.b.c()
    while (tok.type == DOT)
    {
        node = parse_member(std::move(node), ln);
    }
    return node;
}
std::unique_ptr<ASTNode> Parser::parse_member(std::unique_ptr<ASTNode> base, size_t ln)
{
    next_tok();
    std::string mem_name = tok.val;
    expect(IDENT);
    if (tok.type == LPAREN)
    {
        next_tok();
        auto call = std::make_unique<ASTNode>(ASTNode::MEMBER_CALL);
        call->val = mem_name;
        call->line = ln;
        call->children.push_back(std::move(base));
        while (tok.type != RPAREN)
        {
            call->children.push_back(parse_expr());
            if (tok.type == COMMA)
                next_tok();
        }
        expect(RPAREN);
        return call;
    }
    else
    {
        auto acc = std::make_unique<ASTNode>(ASTNode::MEMBER_ACCESS);
        acc->val = mem_name;
        acc->line = ln;
        acc->children.push_back(std::move(base));
        return acc;
    }
}
std::unique_ptr<ASTNode> Parser::parse_class()
{
    size_t ln = tok.line;
    next_tok();
    std::string cls_name = tok.val;
    expect(IDENT);

    std::string super_name;
    if (tok.type == COLON)
    {
        next_tok();
        super_name = tok.val;
        expect(IDENT);
    }
    auto cls_node = std::make_unique<ASTNode>(ASTNode::CLASS_DEF);
    cls_node->val = cls_name;
    cls_node->val2 = super_name;
    cls_node->line = ln;
    expect(T_BEGIN);
    while (tok.type != T_END && tok.type != T_EOF)
    {
        bool is_static = false;
        if (tok.type == T_STATIC)
        {
            is_static = true;
            next_tok();
        }
        expect(T_FUNC);
        size_t fun_ln = tok.line;
        std::string fname = tok.val;
        expect(IDENT);
        expect(LPAREN);
        std::vector<std::string> params;
        bool variadic = false;
        while (tok.type != RPAREN)
        {
            if (tok.type == DOTDOTDOT)
            {
                next_tok();
                if (tok.type != IDENT)
                {
                    std::cerr << "[" << tok.line << "] Syntax error: ... requires identifier\n";
                }
                params.push_back("..." + tok.val);
                variadic = true;
                next_tok();
                break;
            }
            params.push_back(tok.val);
            expect(IDENT);
            if (tok.type == COMMA)
                next_tok();
        }
        expect(RPAREN);

        if (!is_static)
        {
            params.insert(params.begin(), "self");
        }
        auto body = parse_block();
        auto fun_node = std::make_unique<ASTNode>(ASTNode::FUNC_DEF);
        fun_node->val = fname + (is_static ? "|static" : "|instance");
        fun_node->line = fun_ln;
        for (auto &p : params)
        {
            auto vnode = std::make_unique<ASTNode>(ASTNode::VAR);
            vnode->val = p;
            fun_node->children.push_back(std::move(vnode));
        }
        fun_node->children.push_back(std::move(body));
        cls_node->children.push_back(std::move(fun_node));
    }
    expect(T_END);
    return cls_node;
}
std::unique_ptr<ASTNode> Parser::parse_new()
{
    size_t ln = tok.line;
    next_tok();
    std::string clsname = tok.val;
    expect(IDENT);
    expect(LPAREN);
    auto node = std::make_unique<ASTNode>(ASTNode::NEW_OBJ);
    node->val = clsname;
    node->line = ln;
    while (tok.type != RPAREN)
    {
        node->children.push_back(parse_expr());
        if (tok.type == COMMA)
            next_tok();
    }
    expect(RPAREN);
    return node;
}
std::unique_ptr<ASTNode> parse_source(const std::string &src)
{
    Parser p;
    p.lex.src = src;
    p.next_tok();
    return p.parse_program();
}
