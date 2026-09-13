#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <memory>
#include <sstream>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <variant>
#include <cmath>

const int BD_DIV_PRECISION = 50; // Max decimal digits for division

// ===================== BigDecimal arbitrary‑precision decimal (+‑*/ compare) =====================
struct BigDecimal
{
    std::string integer;
    std::string fractional;
    bool negative = false;
    BigDecimal() = default;
    BigDecimal(const std::string &s);
    BigDecimal(long long n);
    void normalize();
    int compare(const BigDecimal &other) const;
    BigDecimal operator+(const BigDecimal &other) const;
    BigDecimal operator-(const BigDecimal &other) const;
    BigDecimal operator*(const BigDecimal &other) const;
    BigDecimal operator/(const BigDecimal &other) const;
    bool operator==(const BigDecimal &o) const { return compare(o) == 0; }
    bool operator!=(const BigDecimal &o) const { return compare(o) != 0; }
    bool operator<(const BigDecimal &o) const { return compare(o) < 0; }
    bool operator>(const BigDecimal &o) const { return compare(o) > 0; }
    bool operator<=(const BigDecimal &o) const { return compare(o) <= 0; }
    bool operator>=(const BigDecimal &o) const { return compare(o) >= 0; }
    std::string to_string() const;
};

BigDecimal::BigDecimal(const std::string &s)
{
    if (s.empty())
    {
        throw std::runtime_error("BigDecimal parse: empty string");
    }
    size_t i = 0;
    if (s[0] == '-')
    {
        negative = true;
        i = 1;
    }
    size_t dot = s.find('.', i);
    if (dot == std::string::npos)
    {
        integer = s.substr(i);
        fractional = "";
    }
    else
    {
        integer = s.substr(i, dot - i);
        fractional = s.substr(dot + 1);
    }
    normalize();
}

BigDecimal::BigDecimal(long long n)
{
    if (n < 0)
    {
        negative = true;
        n = -n;
    }
    integer = std::to_string(n);
    fractional = "";
    normalize();
}

void BigDecimal::normalize()
{
    size_t start = 0;
    while (start + 1 < integer.size() && integer[start] == '0')
        start++;
    integer = integer.substr(start);
    while (!fractional.empty() && fractional.back() == '0')
        fractional.pop_back();
    if (integer.empty() || integer == "0")
    {
        integer = "0";
        negative = false;
    }
}

static std::string add_str(const std::string &a, const std::string &b)
{
    std::string res;
    int carry = 0;
    int i = (int)a.size() - 1, j = (int)b.size() - 1;
    while (i >= 0 || j >= 0 || carry > 0)
    {
        int va = i >= 0 ? a[i--] - '0' : 0;
        int vb = j >= 0 ? b[j--] - '0' : 0;
        int sum = va + vb + carry;
        carry = sum / 10;
        res.push_back((sum % 10) + '0');
    }
    std::reverse(res.begin(), res.end());
    return res;
}

static std::string sub_str(const std::string &a, const std::string &b)
{
    std::string res;
    int borrow = 0;
    int i = (int)a.size() - 1, j = (int)b.size() - 1;
    while (i >= 0)
    {
        int va = a[i--] - '0' - borrow;
        int vb = j >= 0 ? b[j--] - '0' : 0;
        borrow = 0;
        if (va < vb)
        {
            va += 10;
            borrow = 1;
        }
        res.push_back(va - vb + '0');
    }
    std::reverse(res.begin(), res.end());
    size_t st = 0;
    while (st + 1 < res.size() && res[st] == '0')
        st++;
    return res.substr(st);
}

static std::string mul_str(const std::string &a, const std::string &b)
{
    std::vector<int> vec(a.size() + b.size(), 0);
    for (int i = (int)a.size() - 1; i >= 0; i--)
    {
        for (int j = (int)b.size() - 1; j >= 0; j--)
        {
            int p = (a[i] - '0') * (b[j] - '0');
            int sum = vec[i + j + 1] + p;
            vec[i + j + 1] = sum % 10;
            vec[i + j] += sum / 10;
        }
    }
    std::string s;
    bool skip = true;
    for (auto d : vec)
    {
        if (d != 0)
            skip = false;
        if (!skip)
            s.push_back(d + '0');
    }
    if (s.empty())
        s = "0";
    return s;
}

static int cmp_raw(const std::string &a, const std::string &b)
{
    if (a.size() != b.size())
        return a.size() > b.size() ? 1 : -1;
    return a.compare(b);
}

static void div_raw(std::string a, std::string b, std::string &quotient, std::string &rem)
{
    quotient.clear();
    rem.clear();
    for (char ch : a)
    {
        rem.push_back(ch);
        size_t st = 0;
        while (st + 1 < rem.size() && rem[st] == '0')
            st++;
        rem = rem.substr(st);
        if (rem.empty())
            rem = "0";
        int digit = 0;
        while (cmp_raw(rem, b) >= 0)
        {
            rem = sub_str(rem, b);
            digit++;
        }
        quotient.push_back(digit + '0');
    }
    size_t stq = 0;
    while (stq + 1 < quotient.size() && quotient[stq] == '0')
        stq++;
    quotient = quotient.substr(stq);
    if (quotient.empty())
        quotient = "0";
}

int BigDecimal::compare(const BigDecimal &other) const
{
    if (negative != other.negative)
        return negative ? -1 : 1;
    bool neg = negative;
    const std::string &A = integer, &B = other.integer;
    if (A.size() != B.size())
    {
        if (A.size() > B.size())
            return neg ? -1 : 1;
        else
            return neg ? 1 : -1;
    }
    int cmp = A.compare(B);
    if (cmp != 0)
        return neg ? -cmp : cmp;
    size_t maxfrac = std::max(fractional.size(), other.fractional.size());
    for (size_t k = 0; k < maxfrac; k++)
    {
        char ca = k < fractional.size() ? fractional[k] : '0';
        char cb = k < other.fractional.size() ? other.fractional[k] : '0';
        if (ca != cb)
            return neg ? (cb - ca) : (ca - cb);
    }
    return 0;
}

BigDecimal BigDecimal::operator+(const BigDecimal &other) const
{
    BigDecimal res;
    if (negative == other.negative)
    {
        res.negative = negative;
        int maxfrac = std::max(fractional.size(), other.fractional.size());
        std::string fa = fractional;
        fa.resize(maxfrac, '0');
        std::string fb = other.fractional;
        fb.resize(maxfrac, '0');
        std::string fsum = add_str(fa, fb);
        std::string intA = integer, intB = other.integer;
        std::string intsum = add_str(intA, intB);
        if (fsum.size() > (size_t)maxfrac)
        {
            intsum = add_str(intsum, "1");
            fsum = fsum.substr(1);
        }
        res.integer = intsum;
        res.fractional = fsum;
    }
    else
    {
        if (compare(other) >= 0)
        {
            res.negative = negative;
            int maxfrac = std::max(fractional.size(), other.fractional.size());
            std::string fa = fractional;
            fa.resize(maxfrac, '0');
            std::string fb = other.fractional;
            fb.resize(maxfrac, '0');
            std::string fsub = sub_str(fa, fb);
            std::string intA = integer, intB = other.integer;
            std::string intsub = sub_str(intA, intB);
            res.integer = intsub;
            res.fractional = fsub;
        }
        else
        {
            res = other - *this;
            res.negative = !other.negative;
        }
    }
    res.normalize();
    return res;
}

BigDecimal BigDecimal::operator-(const BigDecimal &other) const
{
    BigDecimal neg_other = other;
    neg_other.negative = !neg_other.negative;
    return *this + neg_other;
}

BigDecimal BigDecimal::operator*(const BigDecimal &other) const
{
    BigDecimal res;
    res.negative = (negative != other.negative);
    std::string allA = integer + fractional;
    std::string allB = other.integer + other.fractional;
    std::string full = mul_str(allA, allB);
    int totalFrac = (int)fractional.size() + (int)other.fractional.size();
    int split = (int)full.size() - totalFrac;
    if (split <= 0)
    {
        res.integer = "0";
        res.fractional = std::string(-split, '0') + full;
    }
    else
    {
        res.integer = full.substr(0, split);
        res.fractional = full.substr(split);
    }
    res.normalize();
    return res;
}

BigDecimal BigDecimal::operator/(const BigDecimal &other) const
{
    if (other.compare(BigDecimal("0")) == 0)
    {
        throw std::runtime_error("division by zero");
    }
    BigDecimal a = *this;
    BigDecimal b = other;
    bool out_neg = (a.negative != b.negative);
    a.negative = false;
    b.negative = false;
    std::string num = a.integer + a.fractional;
    std::string den = b.integer + b.fractional;
    int shiftA = (int)a.fractional.size();
    int shiftB = (int)b.fractional.size();
    int norm_shift = BD_DIV_PRECISION + shiftB;
    num.append(norm_shift, '0');
    std::string q, r;
    div_raw(num, den, q, r);
    BigDecimal res;
    int dot_pos = (int)q.size() - norm_shift;
    if (dot_pos <= 0)
    {
        res.integer = "0";
        res.fractional = std::string(-dot_pos, '0') + q;
    }
    else
    {
        res.integer = q.substr(0, dot_pos);
        res.fractional = q.substr(dot_pos);
    }
    res.negative = out_neg;
    res.normalize();
    return res;
}

std::string BigDecimal::to_string() const
{
    std::string out;
    if (negative && integer != "0")
        out += "-";
    out += integer;
    if (!fractional.empty())
        out += "." + fractional;
    return out;
}

// ===================== RuntimeVal (std::variant重构) =====================
class ASTNode;
using FuncT = std::pair<std::vector<std::string>, ASTNode *>;
struct RuntimeVal;
using Array = std::vector<RuntimeVal>;
using Dict = std::map<RuntimeVal, RuntimeVal>;

struct RuntimeVal
{
    enum Type
    {
        NUMBER,
        STRING,
        ARRAY,
        DICT,
        FUNCTION,
        NIL
    };
    Type type;
    std::variant<std::monostate, BigDecimal, std::string, Array, Dict, FuncT> data;

    // constructors
    RuntimeVal() : type(NIL), data(std::monostate{}) {}
    RuntimeVal(BigDecimal n) : type(NUMBER), data(std::move(n)) {}
    RuntimeVal(std::string s) : type(STRING), data(std::move(s)) {}
    RuntimeVal(Array a) : type(ARRAY), data(std::move(a)) {}
    RuntimeVal(Dict d) : type(DICT), data(std::move(d)) {}
    RuntimeVal(FuncT f) : type(FUNCTION), data(std::move(f)) {}

    // getters (const / non‑const)
    BigDecimal &get_num() { return std::get<BigDecimal>(data); }
    const BigDecimal &get_num() const { return std::get<BigDecimal>(data); }

    std::string &get_str() { return std::get<std::string>(data); }
    const std::string &get_str() const { return std::get<std::string>(data); }

    Array &get_arr() { return std::get<Array>(data); }
    const Array &get_arr() const { return std::get<Array>(data); }

    Dict &get_dict() { return std::get<Dict>(data); }
    const Dict &get_dict() const { return std::get<Dict>(data); }

    FuncT &get_func() { return std::get<FuncT>(data); }
    const FuncT &get_func() const { return std::get<FuncT>(data); }

    bool operator<(const RuntimeVal &o) const;
    bool operator==(const RuntimeVal &o) const;
    std::string to_string() const;
};

bool RuntimeVal::operator<(const RuntimeVal &o) const
{
    if (type != o.type)
        return type < o.type;
    switch (type)
    {
    case NUMBER:
        return get_num() < o.get_num();
    case STRING:
        return get_str() < o.get_str();
    default:
        return false;
    }
}

bool RuntimeVal::operator==(const RuntimeVal &o) const
{
    if (type != o.type)
        return false;
    switch (type)
    {
    case NUMBER:
        return get_num() == o.get_num();
    case STRING:
        return get_str() == o.get_str();
    case ARRAY:
        return get_arr() == o.get_arr();
    case DICT:
        return get_dict() == o.get_dict();
    case FUNCTION:
        return false;
    case NIL:
        return true;
    }
    return false;
}

std::string RuntimeVal::to_string() const
{
    switch (type)
    {
    case NUMBER:
        return get_num().to_string();
    case STRING:
        return "\"" + get_str() + "\"";
    case ARRAY:
    {
        std::string res = "[";
        auto &arr = get_arr();
        for (size_t i = 0; i < arr.size(); i++)
        {
            if (i > 0)
                res += ",";
            res += arr[i].to_string();
        }
        res += "]";
        return res;
    }
    case DICT:
    {
        std::string res = "{";
        bool first = true;
        auto &d = get_dict();
        for (auto &p : d)
        {
            if (!first)
                res += ",";
            first = false;
            res += p.first.to_string() + ":" + p.second.to_string();
        }
        res += "}";
        return res;
    }
    case FUNCTION:
        return "<function>";
    case NIL:
        return "<nil>";
    default:
        return "?";
    }
}

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
        CONTINUE
    } kind;
    std::vector<std::unique_ptr<ASTNode>> children;
    std::string val;
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
    ASSIGN,
    LPAREN,
    RPAREN,
    LBRACK,
    RBRACK,
    LBRACE,
    RBRACE,
    COMMA,
    COLON,
    SEMI
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
            if (peek() == '/' && (idx + 1 < src.size()) && src[idx + 1] == '/')
            {
                consume();
                consume();
                while (peek() != 0 && peek() != '\n')
                {
                    consume();
                }
            }
            else
            {
                break;
            }
        }
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
    Token read_str()
    {
        size_t ln = cur_line;
        consume();
        size_t start = idx;
        while (peek() != '"' && peek())
            consume();
        std::string s = src.substr(start, idx - start);
        consume();
        return Token{STR, s, start, ln};
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
            return Token{PLUS, "+", idx - 1, ln};
        }
        if (c == '-')
        {
            consume();
            return Token{MINUS, "-", idx - 1, ln};
        }
        if (c == '*')
        {
            consume();
            return Token{STAR, "*", idx - 1, ln};
        }
        if (c == '/')
        {
            consume();
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
        std::cerr << "[" << ln << "] Lexer error: unknown character '" << c << "'\n";
        consume();
        return next();
    }
};

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
        std::cerr << "[" << tok.line << "] Syntax error: unexpected token\n";
        return false;
    }
    std::unique_ptr<ASTNode> parse_program()
    {
        auto prog = std::make_unique<ASTNode>(ASTNode::PROGRAM);
        prog->line = lex.cur_line;
        while (tok.type != T_EOF)
        {
            prog->children.push_back(parse_stmt());
        }
        return prog;
    }
    std::unique_ptr<ASTNode> parse_stmt();
    std::unique_ptr<ASTNode> parse_if();
    std::unique_ptr<ASTNode> parse_while();
    std::unique_ptr<ASTNode> parse_for_in();
    std::unique_ptr<ASTNode> parse_func();
    std::unique_ptr<ASTNode> parse_block();
    std::unique_ptr<ASTNode> parse_expr();
    std::unique_ptr<ASTNode> parse_compare();
    std::unique_ptr<ASTNode> parse_add();
    std::unique_ptr<ASTNode> parse_mul();
    std::unique_ptr<ASTNode> parse_primary();
    std::unique_ptr<ASTNode> parse_call_or_index(const std::string &name, size_t ln);
};

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
        else if (tok.type == LBRACK)
        {
            next_tok();
            auto idx_expr = parse_expr();
            expect(RBRACK);
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
        node->children.push_back(parse_block());
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
    while (tok.type != RPAREN)
    {
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
    auto blk = std::make_unique<ASTNode>(ASTNode::PROGRAM);
    while (tok.type != T_EOF && tok.type != T_ELSE && tok.type != RBRACE && tok.type != RBRACK)
    {
        blk->children.push_back(parse_stmt());
    }
    return blk;
}

std::unique_ptr<ASTNode> Parser::parse_expr() { return parse_compare(); }

std::unique_ptr<ASTNode> Parser::parse_compare()
{
    auto lhs = parse_add();
    while (true)
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
    auto lhs = parse_primary();
    while (tok.type == STAR || tok.type == SLASH)
    {
        std::string op = tok.val;
        size_t ln = tok.line;
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::BINARY);
        n->val = op;
        n->line = ln;
        n->children.push_back(std::move(lhs));
        n->children.push_back(parse_primary());
        lhs = std::move(n);
    }
    return lhs;
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
        n->literal = BigDecimal(t.val);
        n->line = ln;
        return n;
    }
    case STR:
    {
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::LIT_STR);
        n->literal = t.val;
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
    default:
        std::cerr << "[" << ln << "] Parse error: bad expression\n";
        next_tok();
        return std::make_unique<ASTNode>(ASTNode::LIT_NUM);
    }
}

std::unique_ptr<ASTNode> Parser::parse_call_or_index(const std::string &name, size_t ln)
{
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
        return call;
    }
    else if (tok.type == LBRACK)
    {
        next_tok();
        auto idx = std::make_unique<ASTNode>(ASTNode::INDEX);
        idx->val = name;
        idx->line = ln;
        idx->children.push_back(parse_expr());
        expect(RBRACK);
        return idx;
    }
    else
    {
        auto v = std::make_unique<ASTNode>(ASTNode::VAR);
        v->val = name;
        v->line = ln;
        return v;
    }
}

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
    void set(const std::string &name, RuntimeVal v) { vars[name] = v; }
};

// ===================== Interpreter =====================
struct Interpreter
{
    Scope global;
    RuntimeVal ret_val;
    bool has_return = false;
    bool break_flag = false;
    bool continue_flag = false;
    RuntimeVal *get_lvalue(RuntimeVal &root, ASTNode *idx_node, Scope *scope, size_t line);
    RuntimeVal eval(ASTNode *node, Scope *scope);
};

RuntimeVal *Interpreter::get_lvalue(RuntimeVal &root, ASTNode *idx_node, Scope *scope, size_t line)
{
    RuntimeVal *cur = &root;
    auto idxv = eval(idx_node->children[0].get(), scope);
    if (cur->type == RuntimeVal::ARRAY)
    {
        if (idxv.type != RuntimeVal::NUMBER)
        {
            std::cerr << "[" << line << "] Runtime error: array index must be number\n";
            return nullptr;
        }
        size_t i;
        try
        {
            i = std::stoull(idxv.get_num().to_string());
        }
        catch (...)
        {
            std::cerr << "[" << line << "] Runtime error: invalid index value\n";
            return nullptr;
        }
        if (i >= cur->get_arr().size())
        {
            std::cerr << "[" << line << "] Runtime error: array index out of bounds\n";
            return nullptr;
        }
        return &cur->get_arr()[i];
    }
    else if (cur->type == RuntimeVal::DICT)
    {
        return &(cur->get_dict()[idxv]);
    }
    std::cerr << "[" << line << "] Runtime error: cannot index‑assign non‑array/non‑dict\n";
    return nullptr;
}

RuntimeVal Interpreter::eval(ASTNode *node, Scope *scope)
{
    if (has_return)
        return ret_val;
    if (break_flag || continue_flag)
        return RuntimeVal();
    size_t ln = node->line;
    try
    {
        switch (node->kind)
        {
        case ASTNode::PROGRAM:
        {
            RuntimeVal res;
            for (auto &c : node->children)
            {
                res = eval(c.get(), scope);
                if (break_flag || continue_flag || has_return)
                    break;
            }
            return res;
        }
        case ASTNode::LIT_NUM:
            return node->literal;
        case ASTNode::LIT_STR:
            return node->literal;
        case ASTNode::VAR:
        {
            auto p = scope->get(node->val);
            if (!p)
            {
                std::cerr << "[" << ln << "] Runtime error: undefined variable: " << node->val << "\n";
                return RuntimeVal();
            }
            return *p;
        }
        case ASTNode::ASSIGN:
        {
            auto v = eval(node->children[0].get(), scope);
            scope->set(node->val, v);
            return v;
        }
        case ASTNode::INDEX_ASSIGN:
        {
            auto base_p = scope->get(node->val);
            if (!base_p)
            {
                std::cerr << "[" << ln << "] Runtime error: undefined variable: " << node->val << "\n";
                return RuntimeVal();
            }
            RuntimeVal *lv = get_lvalue(*base_p, node->children[0].get(), scope, ln);
            if (!lv)
                return RuntimeVal();
            auto new_val = eval(node->children[1].get(), scope);
            *lv = new_val;
            return new_val;
        }
        case ASTNode::BINARY:
        {
            auto lhs = eval(node->children[0].get(), scope);
            auto rhs = eval(node->children[1].get(), scope);
            if (lhs.type == RuntimeVal::NUMBER && rhs.type == RuntimeVal::NUMBER)
            {
                auto &lnum = lhs.get_num();
                auto &rnum = rhs.get_num();
                if (node->val == "+")
                    return lnum + rnum;
                if (node->val == "-")
                    return lnum - rnum;
                if (node->val == "*")
                    return lnum * rnum;
                if (node->val == "/")
                    return lnum / rnum;
                if (node->val == "==")
                    return BigDecimal(lnum == rnum ? 1 : 0);
                if (node->val == "!=")
                    return BigDecimal(lnum != rnum ? 1 : 0);
                if (node->val == "<")
                    return BigDecimal(lnum < rnum ? 1 : 0);
                if (node->val == ">")
                    return BigDecimal(lnum > rnum ? 1 : 0);
                if (node->val == "<=")
                    return BigDecimal(lnum <= rnum ? 1 : 0);
                if (node->val == ">=")
                    return BigDecimal(lnum >= rnum ? 1 : 0);
            }
            if (node->val == "+" && lhs.type == RuntimeVal::STRING && rhs.type == RuntimeVal::STRING)
            {
                return lhs.get_str() + rhs.get_str();
            }
            std::cerr << "[" << ln << "] Runtime error: binary operand type mismatch\n";
            return RuntimeVal();
        }
        case ASTNode::IF:
        {
            auto cond = eval(node->children[0].get(), scope);
            if (cond.type == RuntimeVal::NUMBER && !(cond.get_num() == BigDecimal("0")))
            {
                eval(node->children[1].get(), scope);
            }
            else if (node->children.size() >= 3)
            {
                eval(node->children[2].get(), scope);
            }
            return RuntimeVal();
        }
        case ASTNode::WHILE:
        {
            while (true)
            {
                auto cond = eval(node->children[0].get(), scope);
                if (cond.type != RuntimeVal::NUMBER || cond.get_num() == BigDecimal("0"))
                    break;
                eval(node->children[1].get(), scope);
                if (has_return)
                    break;
                if (break_flag)
                {
                    break_flag = false;
                    break;
                }
                if (continue_flag)
                {
                    continue_flag = false;
                    continue;
                }
            }
            return RuntimeVal();
        }
        case ASTNode::FOR_IN:
        {
            auto seq_val = eval(node->children[0].get(), scope);
            if (seq_val.type != RuntimeVal::ARRAY)
            {
                std::cerr << "[" << ln << "] Runtime error: foreach expects array\n";
                return RuntimeVal();
            }
            Scope blk_scope(scope);
            for (auto &item : seq_val.get_arr())
            {
                blk_scope.set(node->val, item);
                eval(node->children[1].get(), &blk_scope);
                if (has_return)
                    break;
                if (break_flag)
                {
                    break_flag = false;
                    break;
                }
                if (continue_flag)
                {
                    continue_flag = false;
                    continue;
                }
            }
            return RuntimeVal();
        }
        case ASTNode::RANGE:
        {
            auto s = eval(node->children[0].get(), scope);
            auto e = eval(node->children[1].get(), scope);
            if (s.type != RuntimeVal::NUMBER || e.type != RuntimeVal::NUMBER)
            {
                std::cerr << "[" << ln << "] Runtime error: range() arguments must be number\n";
                return RuntimeVal(Array{});
            }
            BigDecimal start = s.get_num();
            BigDecimal end = e.get_num();
            BigDecimal step("1");
            if (node->children.size() >= 3)
            {
                auto st = eval(node->children[2].get(), scope);
                if (st.type != RuntimeVal::NUMBER)
                {
                    std::cerr << "[" << ln << "] Runtime error: range() step must be number\n";
                    return RuntimeVal(Array{});
                }
                step = st.get_num();
            }
            Array arr;
            BigDecimal cur = start;
            while (cur < end)
            {
                arr.push_back(RuntimeVal(cur));
                cur = cur + step;
            }
            return RuntimeVal(arr);
        }
        case ASTNode::ARRAY_LIT:
        {
            Array arr;
            for (auto &c : node->children)
                arr.push_back(eval(c.get(), scope));
            return RuntimeVal(arr);
        }
        case ASTNode::DICT_LIT:
        {
            Dict d;
            for (size_t i = 0; i < node->children.size(); i += 2)
            {
                auto k = eval(node->children[i].get(), scope);
                auto v = eval(node->children[i + 1].get(), scope);
                d[k] = v;
            }
            return RuntimeVal(d);
        }
        case ASTNode::INDEX:
        {
            auto base = *scope->get(node->val);
            auto idxv = eval(node->children[0].get(), scope);
            if (base.type == RuntimeVal::ARRAY && idxv.type == RuntimeVal::NUMBER)
            {
                size_t idx = std::stoull(idxv.get_num().to_string());
                if (idx >= base.get_arr().size())
                {
                    std::cerr << "[" << ln << "] Runtime error: array index out of bounds\n";
                    return RuntimeVal();
                }
                return base.get_arr()[idx];
            }
            if (base.type == RuntimeVal::DICT)
            {
                return base.get_dict()[idxv];
            }
            std::cerr << "[" << ln << "] Runtime error: index requires array/dict\n";
            return RuntimeVal();
        }
        case ASTNode::FUNC_DEF:
        {
            std::vector<std::string> params;
            size_t paramCnt = node->children.size() - 1;
            for (size_t i = 0; i < paramCnt; i++)
                params.push_back(node->children[i]->val);
            auto body = node->children.back().get();
            scope->set(node->val, RuntimeVal(std::make_pair(params, body)));
            return RuntimeVal();
        }
        case ASTNode::CALL:
        {
            if (node->val == "output")
            {
                auto arg = eval(node->children[0].get(), scope);
                std::cout << arg.to_string() << "\n";
                return RuntimeVal();
            }
            if (node->val == "input")
            {
                std::string s;
                std::cin >> s;
                return RuntimeVal(s);
            }
            if (node->val == "tonum")
            {
                if (node->children.size() != 1)
                {
                    std::cerr << "[" << ln << "] Runtime error: tonum() expects exactly one argument\n";
                    return RuntimeVal();
                }
                auto arg = eval(node->children[0].get(), scope);
                if (arg.type != RuntimeVal::STRING)
                {
                    std::cerr << "[" << ln << "] Runtime error: tonum() argument must be string\n";
                    return RuntimeVal();
                }
                BigDecimal tempNum;
                try
                {
                    tempNum = BigDecimal(arg.get_str());
                }
                catch (...)
                {
                    std::cerr << "[" << ln << "] Runtime error: tonum() invalid number string: " << arg.get_str() << "\n";
                    return RuntimeVal();
                }
                return RuntimeVal(std::move(tempNum));
            }
            if (node->val == "tostring")
            {
                if (node->children.size() != 1)
                {
                    std::cerr << "[" << ln << "] Runtime error: tostring() expects exactly one argument\n";
                    return RuntimeVal();
                }
                auto arg = eval(node->children[0].get(), scope);
                return RuntimeVal(arg.to_string());
            }
            auto fv = *scope->get(node->val);
            if (fv.type != RuntimeVal::FUNCTION)
            {
                std::cerr << "[" << ln << "] Runtime error: not a function\n";
                return RuntimeVal();
            }
            Scope fscope(scope);
            auto &params = fv.get_func().first;
            for (size_t i = 0; i < params.size(); i++)
            {
                auto arg = eval(node->children[i].get(), scope);
                fscope.set(params[i], arg);
            }
            has_return = false;
            eval(fv.get_func().second, &fscope);
            return ret_val;
        }
        case ASTNode::RETURN:
        {
            ret_val = eval(node->children[0].get(), scope);
            has_return = true;
            return ret_val;
        }
        case ASTNode::BREAK:
            break_flag = true;
            return RuntimeVal();
        case ASTNode::CONTINUE:
            continue_flag = true;
            return RuntimeVal();
        default:
            std::cerr << "[" << ln << "] Runtime error: unknown AST node\n";
            return RuntimeVal();
        }
    }
    catch (std::runtime_error &ex)
    {
        std::cerr << "[" << ln << "] Runtime exception: " << ex.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "[" << ln << "] Runtime unknown exception\n";
    }
    return RuntimeVal();
}
// TODO:增加转义字符，模块导入，变长参数
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Sandiscript interpreter 1.3.alpha\n";
        std::cout << "Usage: Sandi.exe script.sand\n";
        std::cout << "Example: Sandi.exe test.sand\n";
        return 1;
    }
    std::ifstream fin(argv[1]);
    if (!fin.is_open())
    {
        std::cerr << "Error: cannot open file " << argv[1] << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string src = buffer.str();
    fin.close();
    Parser p;
    p.lex.src = src;
    p.next_tok();
    auto ast = p.parse_program();
    Interpreter interp;
    interp.eval(ast.get(), &interp.global);
    return 0;
}
