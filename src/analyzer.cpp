#include "analyzer.h"

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
    case T_DECLARE:
    case T_FORWARD:
        return parse_forward_decl();
    case T_IF:
        return parse_if();
    case T_WHILE:
        return parse_while();
    case T_FOR:
        return parse_for_in();
    case T_FUNC:
        return parse_func();
    case T_WORKER:
        next_tok();
        if (tok.type != T_FUNC)
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << tok.line << "] Syntax error: worker must precede func\n";
            Console::setColor(Console::Color::Reset);
            return std::make_unique<ASTNode>(ASTNode::PROGRAM);
        }
        return parse_func(true);
    case T_CLASS:
        return parse_class();
    case T_SWITCH:
        return parse_switch();
    case T_PROPERTY:
    {
        next_tok();
        if (tok.type != IDENT)
        {
            expect(IDENT);
            return std::make_unique<ASTNode>(ASTNode::WIDGET_PROP);
        }
        auto property_name = std::make_unique<ASTNode>(ASTNode::VAR);
        property_name->val = tok.val;
        property_name->line = tok.line;
        next_tok();
        return parse_widget_prop(std::move(property_name), ln);
    }
    case T_SLOT:
    {
        next_tok();
        if (tok.type != IDENT)
        {
            expect(IDENT);
            return std::make_unique<ASTNode>(ASTNode::SLOT_CONNECT_EXPR);
        }
        const std::string signal_name = tok.val;
        next_tok();
        if (!expect(CONNECT))
            return std::make_unique<ASTNode>(ASTNode::SLOT_CONNECT_EXPR);
        auto node = std::make_unique<ASTNode>(ASTNode::SLOT_CONNECT_EXPR);
        node->val = "qt:" + signal_name;
        node->line = ln;
        auto self = std::make_unique<ASTNode>(ASTNode::VAR);
        self->val = "self";
        self->line = ln;
        node->children.push_back(std::move(self));
        node->children.push_back(parse_expr());
        if (tok.type == SEMI)
            next_tok();
        return node;
    }
    case T_WIDGET:
    {
        next_tok();
        if (tok.type == T_CLASS)
            return parse_widget_class();
        if (tok.type != IDENT)
        {
            expect(IDENT);
            return std::make_unique<ASTNode>(ASTNode::WIDGET_DECL);
        }
        auto widget_type = std::make_unique<ASTNode>(ASTNode::VAR);
        widget_type->val = tok.val;
        widget_type->line = tok.line;
        next_tok();
        return parse_widget(std::move(widget_type), ln);
    }
    // case T_NEW:
    // return parse_new();*extra
    case T_FROM:
    {
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::IMPORT);
        n->line = ln;
        if (tok.type != STR)
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << tok.line << "] Syntax error: expect string filename for from...import\n";
            Console::setColor(Console::Color::Reset);
            return std::make_unique<ASTNode>(ASTNode::PROGRAM);
        }
        n->val = tok.val;
        next_tok();
        expect(T_IMPORT);
        if (tok.type == LBRACE || tok.type == T_BEGIN)
        {
            const SandTokenType closing = block_end(tok.type);
            next_tok();
            n->val2 = "selective";
            while (tok.type != closing && tok.type != T_EOF)
            {
                if (tok.type != IDENT)
                {
                    expect(IDENT);
                    break;
                }
                auto name = std::make_unique<ASTNode>(ASTNode::VAR);
                name->val = tok.val;
                name->line = tok.line;
                n->children.push_back(std::move(name));
                next_tok();
                if (tok.type == COMMA)
                    next_tok();
                else if (tok.type != closing)
                {
                    expect(closing);
                    break;
                }
            }
            expect(closing);
            if (tok.type == SEMI)
                next_tok();
        }
        else
        {
            expect(SEMI);
        }
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
            SandTokenType op = tok.type;
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
                SandTokenType op = tok.type;
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
    case T_EMIT:
    {
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::EMIT_EXPR);
        n->line = tok.line;
        n->children.push_back(parse_expr());
        expect(SEMI);
        return n;
    }
    case T_NAMESPACE:
        return parse_namespace();
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

std::unique_ptr<ASTNode> Parser::parse_widget(std::unique_ptr<ASTNode> lhs, size_t ln)
{
    auto node = std::make_unique<ASTNode>(ASTNode::WIDGET_DECL);
    node->val = lhs->val;
    node->line = ln;
    auto block = parse_block();
    for (auto &child : block->children)
    {
        node->children.push_back(std::move(child));
    }
    return node;
}

std::unique_ptr<ASTNode> Parser::parse_widget_prop(std::unique_ptr<ASTNode> lhs, size_t ln)
{
    auto node = std::make_unique<ASTNode>(ASTNode::WIDGET_PROP);
    node->val = lhs->val;
    node->line = ln;
    expect(ASSIGN);
    node->children.push_back(parse_expr());
    expect(SEMI);
    return node;
}

std::unique_ptr<ASTNode> Parser::parse_forward_decl()
{
    size_t ln = tok.line;
    next_tok();
    if (tok.type == T_FUNC)
    {
        next_tok();
        if (tok.type != IDENT)
            expect(IDENT);
        std::string name = tok.val;
        next_tok();
        expect(LPAREN);
        std::vector<std::unique_ptr<ASTNode>> params;
        while (tok.type != RPAREN)
        {
            if (tok.type == DOTDOTDOT)
            {
                next_tok();
                if (tok.type != IDENT)
                    expect(IDENT);
                auto pd = std::make_unique<ASTNode>(ASTNode::PARAM_DEF);
                auto var = std::make_unique<ASTNode>(ASTNode::VAR);
                var->val = "..." + tok.val;
                pd->children.push_back(std::move(var));
                params.push_back(std::move(pd));
                next_tok();
                break;
            }
            if (tok.type != IDENT)
                expect(IDENT);
            auto pd = std::make_unique<ASTNode>(ASTNode::PARAM_DEF);
            auto var = std::make_unique<ASTNode>(ASTNode::VAR);
            var->val = tok.val;
            pd->children.push_back(std::move(var));
            next_tok();
            if (tok.type == ASSIGN)
            {
                next_tok();
                pd->children.push_back(parse_expr());
            }
            params.push_back(std::move(pd));
            if (tok.type == COMMA)
                next_tok();
        }
        expect(RPAREN);
        if (tok.type == SEMI)
            next_tok();
        auto node = std::make_unique<ASTNode>(ASTNode::FORWARD_DECL);
        node->val = "func:" + name;
        node->line = ln;
        for (auto &param : params)
            node->children.push_back(std::move(param));
        return node;
    }
    if (tok.type == T_CLASS)
    {
        next_tok();
        if (tok.type != IDENT)
            expect(IDENT);
        std::string name = tok.val;
        next_tok();
        std::string base;
        if (tok.type == COLON)
        {
            next_tok();
            if (tok.type != IDENT)
                expect(IDENT);
            base = tok.val;
            next_tok();
        }
        if (tok.type == SEMI)
            next_tok();
        auto node = std::make_unique<ASTNode>(ASTNode::FORWARD_DECL);
        node->val = "class:" + name;
        node->val2 = base;
        node->line = ln;
        return node;
    }
    return std::make_unique<ASTNode>(ASTNode::FORWARD_DECL);
}

std::unique_ptr<ASTNode> Parser::parse_widget_class()
{
    size_t ln = tok.line;
    next_tok();
    if (tok.type != IDENT)
        expect(IDENT);
    std::string name = tok.val;
    next_tok();

    std::string base;
    if (tok.type == COLON)
    {
        next_tok();
        if (tok.type != IDENT)
            expect(IDENT);
        base = tok.val;
        next_tok();
    }

    auto node = std::make_unique<ASTNode>(ASTNode::WIDGET_CLASS_DEF);
    node->val = name;
    node->val2 = base;
    node->line = ln;
    if (tok.type != T_BEGIN && tok.type != LBRACE)
    {
        expect(T_BEGIN);
        return node;
    }
    const SandTokenType closing = block_end(tok.type);
    next_tok();
    while (tok.type != closing && tok.type != T_EOF)
    {
        if (tok.type != T_EXPORT)
        {
            auto bad = parse_stmt();
            if (bad)
                node->children.push_back(std::move(bad));
            continue;
        }
        next_tok();
        if (tok.type == T_PROPERTY)
        {
            next_tok();
            if (tok.type != IDENT)
                expect(IDENT);
            auto prop = std::make_unique<ASTNode>(ASTNode::WIDGET_EXPORT_PROP);
            prop->val = tok.val;
            prop->line = tok.line;
            next_tok();
            expect(ASSIGN);
            prop->children.push_back(parse_expr());
            if (tok.type == SEMI)
                next_tok();
            node->children.push_back(std::move(prop));
        }
        else if (tok.type == T_WIDGET)
        {
            next_tok();
            if (tok.type != IDENT)
                expect(IDENT);
            std::string widgetName = tok.val;
            next_tok();
            auto exportWidget = std::make_unique<ASTNode>(ASTNode::WIDGET_EXPORT_WIDGET);
            exportWidget->val = widgetName;
            exportWidget->line = tok.line;
            auto block = parse_block();
            for (auto &child : block->children)
                exportWidget->children.push_back(std::move(child));
            node->children.push_back(std::move(exportWidget));
        }
        else
        {
            if (tok.type == IDENT && tok.val == "signal")
            {
                next_tok();
                if (tok.type != IDENT)
                    expect(IDENT);
                auto signal = std::make_unique<ASTNode>(ASTNode::WIDGET_EXPORT_SIGNAL);
                signal->val = tok.val;
                signal->line = tok.line;
                next_tok();
                if (tok.type == SEMI)
                    next_tok();
                node->children.push_back(std::move(signal));
            }
            else
            {
                next_tok();
            }
        }
    }
    expect(closing);
    return node;
}

std::unique_ptr<ASTNode> Parser::parse_namespace()
{
    size_t ln = tok.line;
    next_tok();
    std::string ns_name = tok.val;
    expect(IDENT);
    auto ns_node = std::make_unique<ASTNode>(ASTNode::NAMESPACE_DEF);
    ns_node->val = ns_name;
    ns_node->line = ln;
    auto block = parse_block();
    for (auto &ch : block->children)
    {
        ns_node->children.push_back(std::move(ch));
    }
    return ns_node;
}

std::unique_ptr<ASTNode> Parser::parse_slot_op(std::unique_ptr<ASTNode> lhs, size_t ln)
{
    SandTokenType op = tok.type;
    next_tok();
    std::unique_ptr<ASTNode> node;
    if (op == CONNECT)
    {
        node = std::make_unique<ASTNode>(ASTNode::SLOT_CONNECT_EXPR);
        node->line = ln;
        node->children.push_back(std::move(lhs));
        auto rhs = parse_logic_or();
        node->children.push_back(std::move(rhs));
    }
    else // DISCONNECT (!>)
    {
        node = std::make_unique<ASTNode>(ASTNode::SLOT_DISCONNECT_EXPR);
        node->line = ln;
        node->children.push_back(std::move(lhs));
        if (tok.type == IDENT && tok.val == "all")
        {
            node->val = "all";
            next_tok();
        }
        else
        {
            node->val = "slot";
            auto rhs = parse_logic_or();
            node->children.push_back(std::move(rhs));
        }
    }
    return node;
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
    if (tok.type != T_BEGIN && tok.type != LBRACE)
    {
        expect(T_BEGIN);
        return std::make_unique<ASTNode>(ASTNode::SWITCH);
    }
    const SandTokenType closing = block_end(tok.type);
    next_tok();

    auto switch_node = std::make_unique<ASTNode>(ASTNode::SWITCH);
    switch_node->line = ln;
    switch_node->children.push_back(std::move(subject_expr));

    std::unordered_set<std::string> case_literals;
    bool seen_default = false;

    while (tok.type != closing && tok.type != T_EOF)
    {
        size_t case_ln = tok.line;
        bool is_default = (tok.type == T_DEFAULT);
        if (is_default)
        {
            next_tok();
            if (seen_default)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << case_ln << "] Syntax error: multiple default in one switch\n";
                Console::setColor(Console::Color::Reset);
            }
            seen_default = true;
        }
        else if (tok.type == T_CASE)
        {
            next_tok();
        }
        else
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << tok.line << "] Syntax error: expect case / default inside switch begin\n";
            Console::setColor(Console::Color::Reset);
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
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << case_ln << "] Syntax error: duplicate case literal " << lit_key << "\n";
                    Console::setColor(Console::Color::Reset);
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
    expect(closing);
    return switch_node;
}
std::unique_ptr<ASTNode> Parser::parse_lambda()
{
    size_t ln = tok.line;
    next_tok();
    expect(LPAREN);
    std::vector<std::unique_ptr<ASTNode>> paramNodes;
    while (tok.type != RPAREN)
    {
        if (tok.type == DOTDOTDOT)
        {
            next_tok();
            if (tok.type != IDENT)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << tok.line << "] Syntax error: ... requires identifier\n";
                Console::setColor(Console::Color::Reset);
            }
            auto pd = std::make_unique<ASTNode>(ASTNode::PARAM_DEF);
            auto v = std::make_unique<ASTNode>(ASTNode::VAR);
            v->val = "..." + tok.val;
            pd->children.push_back(std::move(v));
            paramNodes.push_back(std::move(pd));
            next_tok();
            break;
        }
        std::string pname = tok.val;
        expect(IDENT);
        auto pd = std::make_unique<ASTNode>(ASTNode::PARAM_DEF);
        auto v = std::make_unique<ASTNode>(ASTNode::VAR);
        v->val = pname;
        pd->children.push_back(std::move(v));
        if (tok.type == ASSIGN)
        {
            next_tok();
            auto defExpr = parse_expr();
            pd->children.push_back(std::move(defExpr));
        }
        paramNodes.push_back(std::move(pd));
        if (tok.type == COMMA)
            next_tok();
    }
    expect(RPAREN);
    auto block = parse_block();
    auto check_no_nested_func = [&](auto &&self, ASTNode *node) -> bool
    {
        if (node->kind == ASTNode::FUNC_DEF || node->kind == ASTNode::LAMBDA_EXPR)
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << node->line << "] Syntax error: nested function/lambda is not allowed\n";
            Console::setColor(Console::Color::Reset);
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
    for (auto &pn : paramNodes)
    {
        node->children.push_back(std::move(pn));
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
std::unique_ptr<ASTNode> Parser::parse_func(bool is_worker)
{
    size_t ln = tok.line;
    next_tok();
    std::string name = tok.val;
    expect(IDENT);
    expect(LPAREN);
    std::vector<std::unique_ptr<ASTNode>> paramNodes;
    while (tok.type != RPAREN)
    {
        if (tok.type == DOTDOTDOT)
        {
            next_tok();
            if (tok.type != IDENT)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << tok.line << "] Syntax error: ... requires identifier\n";
                Console::setColor(Console::Color::Reset);
            }
            auto pd = std::make_unique<ASTNode>(ASTNode::PARAM_DEF);
            auto varNode = std::make_unique<ASTNode>(ASTNode::VAR);
            varNode->val = "..." + tok.val;
            pd->children.push_back(std::move(varNode));
            paramNodes.push_back(std::move(pd));
            next_tok();
            break;
        }

        std::string pname = tok.val;
        expect(IDENT);
        auto pd = std::make_unique<ASTNode>(ASTNode::PARAM_DEF);
        auto varNode = std::make_unique<ASTNode>(ASTNode::VAR);
        varNode->val = pname;
        pd->children.push_back(std::move(varNode));

        if (tok.type == ASSIGN)
        {
            next_tok();
            auto defExpr = parse_expr();
            pd->children.push_back(std::move(defExpr));
        }
        paramNodes.push_back(std::move(pd));
        if (tok.type == COMMA)
            next_tok();
    }
    expect(RPAREN);
    auto body = parse_block();
    auto node = std::make_unique<ASTNode>(ASTNode::FUNC_DEF);
    node->val = name;
    node->val2 = is_worker ? "worker" : "";
    node->line = ln;
    for (auto &pn : paramNodes)
    {
        node->children.push_back(std::move(pn));
    }
    node->children.push_back(std::move(body));
    return node;
}
std::unique_ptr<ASTNode> Parser::parse_block()
{
    if (tok.type != T_BEGIN && tok.type != LBRACE)
    {
        expect(T_BEGIN);
        return std::make_unique<ASTNode>(ASTNode::PROGRAM);
    }
    const SandTokenType closing = block_end(tok.type);
    next_tok();
    auto blk = std::make_unique<ASTNode>(ASTNode::PROGRAM);
    while (tok.type != T_EOF && tok.type != closing)
    {
        blk->children.push_back(parse_stmt());
    }
    if (!expect(closing))
    {
        Console::setColor(Console::Color::Red);
        std::cerr << "[" << tok.line << "] Syntax error: missing block terminator\n";
        Console::setColor(Console::Color::Reset);
    }
    return blk;
}
std::unique_ptr<ASTNode> Parser::parse_expr()
{
    auto lhs = parse_logic_or();
    while (tok.type == CONNECT || tok.type == DISCONNECT)
    {
        lhs = parse_slot_op(std::move(lhs), tok.line);
    }
    return lhs;
}
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
    auto lhs = parse_range();
    while (tok.type == AND_AND)
    {
        std::string op = tok.val;
        size_t ln = tok.line;
        next_tok();
        auto n = std::make_unique<ASTNode>(ASTNode::BINARY);
        n->val = op;
        n->line = ln;
        n->children.push_back(std::move(lhs));
        n->children.push_back(parse_range());
        lhs = std::move(n);
    }
    return lhs;
}
std::unique_ptr<ASTNode> Parser::parse_range()
{
    auto lhs = parse_compare();
    if (tok.type != DOTDOTDOT)
        return lhs;

    size_t ln = tok.line;
    next_tok();
    auto range = std::make_unique<ASTNode>(ASTNode::RANGE);
    range->line = ln;
    range->children.push_back(std::move(lhs));
    range->children.push_back(parse_compare());
    return range;
}
std::unique_ptr<ASTNode> Parser::parse_compare()
{
    auto lhs = parse_add();
    for (;;)
    {
        SandTokenType op = tok.type;
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
        SandTokenType op = tok.type;
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
        Console::setColor(Console::Color::Red);
        std::cerr << "[" << ln << "] Parse error: bad expression\n";
        Console::setColor(Console::Color::Reset);
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
    if (tok.type != T_BEGIN && tok.type != LBRACE)
    {
        expect(T_BEGIN);
        return cls_node;
    }
    const SandTokenType closing = block_end(tok.type);
    next_tok();
    while (tok.type != closing && tok.type != T_EOF)
    {
        bool is_static = false;
        if (tok.type == T_STATIC)
        {
            is_static = true;
            next_tok();
        }
        auto fun_node = parse_func();
        std::string real_func_name = fun_node->val;
        fun_node->val = real_func_name + (is_static ? "|static" : "|instance");

        if (!is_static)
        {
            auto selfPd = std::make_unique<ASTNode>(ASTNode::PARAM_DEF);
            auto varSelf = std::make_unique<ASTNode>(ASTNode::VAR);
            varSelf->val = "self";
            selfPd->children.push_back(std::move(varSelf));
            fun_node->children.insert(fun_node->children.begin(), std::move(selfPd));
        }

        cls_node->children.push_back(std::move(fun_node));
    }
    expect(closing);
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