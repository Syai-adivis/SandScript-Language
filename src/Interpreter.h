#include "analyzer.h"

static bool safe_to_size_t(const BigDecimal &num, size_t &out)
{
    if (num.negative)
        return false;
    std::string s = num.to_string();
    try
    {
        unsigned long long v = std::stoull(s);
        out = static_cast<size_t>(v);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

template <typename MapT>
auto map_find_const(MapT &m, const typename MapT::key_type &key)
{
    auto it = m.lower_bound(key);
    if (it != m.end() && !m.key_comp()(key, it->first))
    {
        return it;
    }
    return m.end();
}

struct EvalFrame
{
    bool has_return = false;
    RuntimeVal ret_val;

    bool break_flag = false;
    bool continue_flag = false;

    EvalFrame make_child() const
    {
        EvalFrame cf{};
        return cf;
    }

    static EvalFrame make_expr_frame()
    {
        EvalFrame ef{};
        return ef;
    }

    void reset_control()
    {
        has_return = false;
        ret_val = RuntimeVal();
        break_flag = false;
        continue_flag = false;
    }
};

struct Interpreter
{
    Scope global;

    FuncT *lookup_instance_method_from(ClassMeta *start_meta, const std::string &name);
    FuncT *lookup_instance_method(ClassMeta *meta, const std::string &name);
    FuncT *lookup_static_method(ClassMeta *meta, const std::string &name);
    std::shared_ptr<ClassMeta> resolve_superclass(const std::string &super_name, Scope *scope, size_t line);
    std::vector<std::string> cmd_args;
    RuntimeVal *get_lvalue(RuntimeVal &root, ASTNode *idx_node, Scope *scope, EvalFrame &expr_frame, size_t line);

    void import_file(const std::string &path);

    RuntimeVal eval(ASTNode *node, Scope *scope, EvalFrame &frame);
};

RuntimeVal *Interpreter::get_lvalue(RuntimeVal &root, ASTNode *idx_node, Scope *scope, EvalFrame &expr_frame, size_t line)
{
    auto *arr_ptr = root.as_array();
    if (arr_ptr != nullptr)
    {
        auto idxv = eval(idx_node->children[0].get(), scope, expr_frame);
        auto *num_ptr = idxv.as_num();
        if (!num_ptr)
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << line << "] Runtime error: array index must be number\n";
            Console::setColor(Console::Color::Reset);
            return nullptr;
        }
        size_t i;
        if (!safe_to_size_t(num_ptr->value, i))
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << line << "] Runtime error: invalid index value (must be non‑negative small integer)\n";
            Console::setColor(Console::Color::Reset);
            return nullptr;
        }
        if (i >= arr_ptr->value.size())
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << line << "] Runtime error: array index out of bounds\n";
            Console::setColor(Console::Color::Reset);
            return nullptr;
        }
        return &arr_ptr->value[i];
    }
    auto *dict_ptr = root.as_dict();
    if (dict_ptr != nullptr)
    {
        auto idxv = eval(idx_node->children[0].get(), scope, expr_frame);
        auto it = map_find_const(dict_ptr->value, idxv);
        if (it == dict_ptr->value.end())
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << line << "] Runtime error: dict key not found\n";
            Console::setColor(Console::Color::Reset);
            return nullptr;
        }
        return &it->second;
    }
    Console::setColor(Console::Color::Red);
    std::cerr << "[" << line << "] Runtime error: cannot index‑assign non‑array/non‑dict\n";
    Console::setColor(Console::Color::Reset);
    return nullptr;
}

FuncT *Interpreter::lookup_instance_method(ClassMeta *meta, const std::string &name)
{
    if (!meta)
        return nullptr;
    auto it = meta->instance_methods.find(name);
    if (it != meta->instance_methods.end())
    {
        return &it->second;
    }
    if (meta->super_meta)
    {
        return lookup_instance_method(meta->super_meta.get(), name);
    }
    return nullptr;
}

FuncT *Interpreter::lookup_instance_method_from(ClassMeta *start_meta, const std::string &name)
{
    ClassMeta *cur = start_meta;
    while (cur != nullptr)
    {
        auto it = cur->instance_methods.find(name);
        if (it != cur->instance_methods.end())
        {
            return &it->second;
        }
        cur = cur->super_meta.get();
    }
    return nullptr;
}

FuncT *Interpreter::lookup_static_method(ClassMeta *meta, const std::string &name)
{
    if (!meta)
        return nullptr;
    auto it = meta->static_methods.find(name);
    if (it != meta->static_methods.end())
    {
        return &it->second;
    }
    if (meta->super_meta)
    {
        return lookup_static_method(meta->super_meta.get(), name);
    }
    return nullptr;
}

std::shared_ptr<ClassMeta> Interpreter::resolve_superclass(const std::string &super_name, Scope *scope, size_t line)
{
    if (super_name.empty())
        return nullptr;
    RuntimeVal *super_val = scope->get(super_name);
    if (!super_val)
    {
        Console::setColor(Console::Color::Red);
        std::cerr << "[" << line << "] Runtime error: super class '" << super_name << "' not defined\n";
        Console::setColor(Console::Color::Reset);
        return nullptr;
    }
    auto *super_meta_ptr = super_val->as_classmeta();
    if (!super_meta_ptr)
    {
        Console::setColor(Console::Color::Red);
        std::cerr << "[" << line << "] Runtime error: '" << super_name << "' is not a class\n";
        Console::setColor(Console::Color::Reset);
        return nullptr;
    }
    return super_meta_ptr->value;
}

void Interpreter::import_file(const std::string &path)
{
    std::ifstream fin(path);
    if (!fin.is_open())
    {
        Console::setColor(Console::Color::Red);
        std::cerr << "Import error: cannot open file " << path << "\n";
        Console::setColor(Console::Color::Reset);
        return;
    }
    std::stringstream buf;
    buf << fin.rdbuf();
    fin.close();
    auto ast = parse_source(buf.str());
    EvalFrame subframe;
    eval(ast.get(), &global, subframe);
}

RuntimeVal Interpreter::eval(ASTNode *node, Scope *scope, EvalFrame &frame)
{

    if (frame.break_flag || frame.continue_flag)
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
                res = eval(c.get(), scope, frame);
                if (frame.has_return || frame.break_flag || frame.continue_flag)
                {
                    break;
                }
            }
            return res;
        }
        case ASTNode::IMPORT:
        {
            import_file(node->val);
            return RuntimeVal();
        }
        case ASTNode::LIT_NUM:
        {
            const auto *numlit = node->literal.as_num();
            return RuntimeVal(numlit->value);
        }
        case ASTNode::LIT_STR:
        {
            const auto *strlit = node->literal.as_str();
            return RuntimeVal(strlit->value);
        }
        case ASTNode::VAR:
        {
            auto p = scope->get(node->val);
            if (!p)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: undefined variable: " << node->val << "\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            return p->clone();
        }
        case ASTNode::NAMESPACE_DEF:
        {
            std::string ns_name = node->val;
            size_t ln = node->line;
            auto ns_obj = std::make_shared<ObjectInstance>(nullptr, std::unordered_map<std::string, RuntimeVal>{});
            RuntimeVal ns_val(ns_obj);
            Scope ns_scope(scope);
            EvalFrame ns_frame = frame.make_child();
            for (auto &stmt : node->children)
            {
                ASTNode *s = stmt.get();
                if (s->kind == ASTNode::ASSIGN)
                {
                    EvalFrame ef = EvalFrame::make_expr_frame();
                    auto val = eval(s->children[0].get(), &ns_scope, ef);
                    std::string ident = s->val;
                    ns_obj->members[ident] = std::move(val);
                    continue;
                }
                if (s->kind == ASTNode::FUNC_DEF)
                {
                    std::string fname = s->val;
                    std::vector<FuncParamInfo> paramsInfo;
                    size_t paramCnt = s->children.size() - 1;
                    for (size_t i = 0; i < paramCnt; ++i)
                    {
                        ASTNode *pd = s->children[i].get();
                        FuncParamInfo pi;
                        pi.name = pd->children[0]->val;
                        if (pd->children.size() >= 2)
                        {
                            pi.default_expr = pd->children[1].get();
                        }
                        paramsInfo.push_back(std::move(pi));
                    }
                    ASTNode *body = s->children.back().get();
                    FuncT ft = std::make_pair(std::move(paramsInfo), body);
                    ns_obj->members[fname] = RuntimeVal(std::move(ft));
                    continue;
                }
                if (s->kind == ASTNode::CLASS_DEF)
                {
                    auto meta = std::make_shared<ClassMeta>();
                    meta->name = s->val;
                    meta->super_class_name = s->val2;
                    meta->super_meta = resolve_superclass(meta->super_class_name, &ns_scope, s->line);
                    for (auto &child : s->children)
                    {
                        if (child->kind != ASTNode::FUNC_DEF)
                            continue;
                        std::string tag = child->val;
                        auto sep = tag.find('|');
                        std::string fname = tag.substr(0, sep);
                        std::string mode = tag.substr(sep + 1);
                        FuncT ft;
                        std::vector<FuncParamInfo> paramsInfo;
                        size_t paramCnt = child->children.size() - 1;
                        for (size_t i = 0; i < paramCnt; ++i)
                        {
                            ASTNode *pd = child->children[i].get();
                            FuncParamInfo pi;
                            pi.name = pd->children[0]->val;
                            if (pd->children.size() >= 2)
                            {
                                pi.default_expr = pd->children[1].get();
                            }
                            paramsInfo.push_back(std::move(pi));
                        }
                        ASTNode *body = child->children.back().get();
                        ft = std::make_pair(std::move(paramsInfo), body);
                        if (mode == "instance")
                        {
                            meta->instance_methods[fname] = ft;
                        }
                        else if (mode == "static")
                        {
                            meta->static_methods[fname] = ft;
                        }
                    }
                    std::string cls_name = s->val;
                    ns_obj->members[cls_name] = RuntimeVal(meta);
                    ns_scope.set(cls_name, RuntimeVal(meta));
                    continue;
                }
                eval(s, &ns_scope, ns_frame);
                if (ns_frame.has_return || ns_frame.break_flag || ns_frame.continue_flag)
                    break;
            }
            scope->set(ns_name, std::move(ns_val));
            return RuntimeVal();
        }
        case ASTNode::ASSIGN:
        {
            EvalFrame expr_frame = EvalFrame::make_expr_frame();
            auto v = eval(node->children[0].get(), scope, expr_frame);
            scope->set(node->val, std::move(v));
            return RuntimeVal();
        }
        case ASTNode::INDEX_ASSIGN:
        {
            auto base_p = scope->get(node->val);
            if (!base_p)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: undefined variable: " << node->val << "\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            EvalFrame idx_frame = EvalFrame::make_expr_frame();
            RuntimeVal *lv = get_lvalue(*base_p, node->children[0].get(), scope, idx_frame, ln);
            if (!lv)
                return RuntimeVal();

            EvalFrame rhs_frame = EvalFrame::make_expr_frame();
            auto new_val = eval(node->children[1].get(), scope, rhs_frame);
            *lv = std::move(new_val);
            return RuntimeVal();
        }
        case ASTNode::MEMBER_ASSIGN:
        {
            EvalFrame base_frame = EvalFrame::make_expr_frame();
            auto base_val = eval(node->children[0].get(), scope, base_frame);

            EvalFrame rhs_frame = EvalFrame::make_expr_frame();
            auto rhs_val = eval(node->children[1].get(), scope, rhs_frame);

            std::string mem_name = node->val;
            auto *obj_ptr = base_val.as_object();
            if (obj_ptr != nullptr)
            {
                obj_ptr->value->members[mem_name] = std::move(rhs_val);
                return RuntimeVal();
            }
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << ln << "] Runtime error: member assign requires object instance\n";
            Console::setColor(Console::Color::Reset);
            return RuntimeVal();
        }
        case ASTNode::COMPOUND_ASSIGN:
        {
            std::string full = node->val;
            size_t colon = full.find(':');
            std::string op = full.substr(colon + 1);
            if (full.substr(0, 5) == "INDEX")
            {
                auto &idxNode = node->children[0];
                auto &rhsNode = node->children[1];
                auto &baseNode = node->children[2];

                EvalFrame b_frame = EvalFrame::make_expr_frame();
                RuntimeVal base = eval(baseNode.get(), scope, b_frame);

                EvalFrame i_frame = EvalFrame::make_expr_frame();
                RuntimeVal idxVal = eval(idxNode.get(), scope, i_frame);

                EvalFrame r_frame = EvalFrame::make_expr_frame();
                RuntimeVal rhsVal = eval(rhsNode.get(), scope, r_frame);

                auto *arr = base.as_array();
                auto *idxnum = idxVal.as_num();
                auto *rnum = rhsVal.as_num();
                if (arr && idxnum && rnum)
                {
                    size_t i;
                    if (!safe_to_size_t(idxnum->value, i) || i >= arr->value.size())
                    {
                        Console::setColor(Console::Color::Red);
                        std::cerr << "[" << ln << "] Runtime error: index invalid\n";
                        Console::setColor(Console::Color::Reset);
                        return RuntimeVal();
                    }
                    auto &lval = arr->value[i];
                    auto *lnum = lval.as_num();
                    if (!lnum)
                    {
                        Console::setColor(Console::Color::Red);
                        std::cerr << "[" << ln << "] Runtime error: compound assign requires number\n";
                        Console::setColor(Console::Color::Reset);
                        return RuntimeVal();
                    }
                    BigDecimal res;
                    if (op == "+=")
                        res = lnum->value + rnum->value;
                    else if (op == "-=")
                        res = lnum->value - rnum->value;
                    else if (op == "*=")
                        res = lnum->value * rnum->value;
                    else if (op == "/=")
                        res = lnum->value / rnum->value;
                    lval = std::move(RuntimeVal(res));
                    return RuntimeVal();
                }
                return RuntimeVal();
            }
            else
            {
                std::string varname = full.substr(0, colon);
                auto pv = scope->get(varname);
                if (!pv)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] undefined var " << varname << "\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }

                EvalFrame rhs_frame = EvalFrame::make_expr_frame();
                RuntimeVal rhs = eval(node->children[0].get(), scope, rhs_frame);

                auto *pv_num = pv->as_num();
                auto *rhs_num = rhs.as_num();
                if (!pv_num || !rhs_num)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: compound assign requires number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                BigDecimal res;
                if (op == "+=")
                    res = pv_num->value + rhs_num->value;
                else if (op == "-=")
                    res = pv_num->value - rhs_num->value;
                else if (op == "*=")
                    res = pv_num->value * rhs_num->value;
                else if (op == "/=")
                    res = pv_num->value / rhs_num->value;
                *pv = std::move(RuntimeVal(res));
                return RuntimeVal();
            }
        }
        case ASTNode::INCDEC:
        {
            std::string v = node->val;
            if (v.substr(0, 5) == "INDEX")
            {
                std::string op = v.substr(6);
                auto &idxNode = node->children[0];
                auto &baseNode = node->children[1];

                EvalFrame b_frame = EvalFrame::make_expr_frame();
                RuntimeVal base = eval(baseNode.get(), scope, b_frame);

                EvalFrame i_frame = EvalFrame::make_expr_frame();
                RuntimeVal idxVal = eval(idxNode.get(), scope, i_frame);

                auto *arr = base.as_array();
                auto *idxnum = idxVal.as_num();
                if (!arr || !idxnum)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: index invalid for ++/--\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                size_t i;
                if (!safe_to_size_t(idxnum->value, i) || i >= arr->value.size())
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: index invalid for ++/--\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                RuntimeVal &lv = arr->value[i];
                auto *lv_num = lv.as_num();
                if (!lv_num)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: ++/-- only for number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                if (op == "++")
                    lv_num->value = lv_num->value + BigDecimal("1");
                else
                    lv_num->value = lv_num->value - BigDecimal("1");
                return RuntimeVal(lv_num->value);
            }
            else
            {
                bool is_inc = (v.find("++") != std::string::npos);
                std::string varname = v.substr(0, v.size() - 2);
                auto pv = scope->get(varname);
                auto *pv_num = pv ? pv->as_num() : nullptr;
                if (!pv || !pv_num)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: ++/-- only for number variable\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                if (is_inc)
                    pv_num->value = pv_num->value + BigDecimal("1");
                else
                    pv_num->value = pv_num->value - BigDecimal("1");
                return RuntimeVal(pv_num->value);
            }
        }
        case ASTNode::SLOT_CONNECT_EXPR:
        {
            EvalFrame lhs_frame = EvalFrame::make_expr_frame();
            auto lhsVal = eval(node->children[0].get(), scope, lhs_frame);
            auto *lhsObj = lhsVal.as_object();
            if (!lhsObj)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: connect(>>) left‑hand side must be object\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            EvalFrame slot_frame = EvalFrame::make_expr_frame();
            auto slotVal = eval(node->children[1].get(), scope, slot_frame);
            auto *fptr = slotVal.as_func();
            if (!fptr)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: connect(>>) right‑hand side must be function(slot)\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            lhsObj->value->signalSlots.connections.push_back(fptr->value);
            return RuntimeVal();
        }
        case ASTNode::SLOT_DISCONNECT_EXPR:
        {
            EvalFrame lhs_frame = EvalFrame::make_expr_frame();
            auto lhsVal = eval(node->children[0].get(), scope, lhs_frame);
            auto *lhsObj = lhsVal.as_object();
            if (!lhsObj)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: disconnect(!>) left‑hand side must be object\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            auto &conn = lhsObj->value->signalSlots.connections;
            if (node->val == "all")
            {
                conn.clear();
            }
            else
            {
                EvalFrame slot_frame = EvalFrame::make_expr_frame();
                auto slotVal = eval(node->children[1].get(), scope, slot_frame);
                auto *fptr = slotVal.as_func();
                if (!fptr)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: disconnect(!>) argument must be function\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                ASTNode *targetBody = fptr->value.second;
                conn.erase(std::remove_if(conn.begin(), conn.end(),
                                          [&](const FuncT &ft)
                                          {
                                              return ft.second == targetBody;
                                          }),
                           conn.end());
            }
            return RuntimeVal();
        }
        case ASTNode::EMIT_EXPR:
        {
            EvalFrame obj_frame = EvalFrame::make_expr_frame();
            auto signalObjVal = eval(node->children[0].get(), scope, obj_frame);
            auto *signalObj = signalObjVal.as_object();
            if (!signalObj)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: emit requires object instance\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            auto snapshot = signalObj->value->signalSlots.connections;
            for (auto &ft : snapshot)
            {
                Scope fscope(scope);
                ASTNode *bodyAst = ft.second;
                EvalFrame child_frame = frame.make_child();
                eval(bodyAst, &fscope, child_frame);
            }
            return RuntimeVal();
        }
        case ASTNode::UNARY:
        {
            EvalFrame sub_frame = EvalFrame::make_expr_frame();
            auto sub = eval(node->children[0].get(), scope, sub_frame);
            if (node->val == "!" || node->val == "not")
            {
                auto *subnum = sub.as_num();
                if (!subnum)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: not expects number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                return RuntimeVal((subnum->value == BigDecimal("0")) ? BigDecimal("1") : BigDecimal("0"));
            }
            else if (node->val == "-")
            {
                auto *subnum = sub.as_num();
                if (!subnum)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: unary minus expects number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                BigDecimal n = subnum->value;
                n.negative = !n.negative;
                return RuntimeVal(n);
            }
            return RuntimeVal();
        }
        case ASTNode::BINARY:
        {
            if (node->val == "&&" || node->val == "and")
            {
                EvalFrame lhs_frame = EvalFrame::make_expr_frame();
                auto lhs = eval(node->children[0].get(), scope, lhs_frame);
                auto *lnum = lhs.as_num();
                if (!lnum)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: and requires number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                if (lnum->value == BigDecimal("0"))
                    return RuntimeVal(BigDecimal("0"));

                EvalFrame rhs_frame = EvalFrame::make_expr_frame();
                auto rhs = eval(node->children[1].get(), scope, rhs_frame);
                auto *rnum = rhs.as_num();
                if (!rnum)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: and requires number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                return RuntimeVal((rnum->value == BigDecimal("0")) ? BigDecimal("0") : BigDecimal("1"));
            }
            if (node->val == "||" || node->val == "or")
            {
                EvalFrame lhs_frame = EvalFrame::make_expr_frame();
                auto lhs = eval(node->children[0].get(), scope, lhs_frame);
                auto *lnum = lhs.as_num();
                if (!lnum)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: or requires number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                if (!(lnum->value == BigDecimal("0")))
                    return RuntimeVal(BigDecimal("1"));

                EvalFrame rhs_frame = EvalFrame::make_expr_frame();
                auto rhs = eval(node->children[1].get(), scope, rhs_frame);
                auto *rnum = rhs.as_num();
                if (!rnum)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: or requires number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                return RuntimeVal((rnum->value == BigDecimal("0")) ? BigDecimal("0") : BigDecimal("1"));
            }

            EvalFrame lhs_frame = EvalFrame::make_expr_frame();
            auto lhs = eval(node->children[0].get(), scope, lhs_frame);

            EvalFrame rhs_frame = EvalFrame::make_expr_frame();
            auto rhs = eval(node->children[1].get(), scope, rhs_frame);

            auto *lnum = lhs.as_num();
            auto *rnum = rhs.as_num();
            if (lnum && rnum)
            {
                auto &lv = lnum->value;
                auto &rv = rnum->value;
                if (node->val == "+")
                    return RuntimeVal(lv + rv);
                if (node->val == "-")
                    return RuntimeVal(lv - rv);
                if (node->val == "*")
                    return RuntimeVal(lv * rv);
                if (node->val == "/")
                    return RuntimeVal(lv / rv);
                if (node->val == "==")
                    return RuntimeVal(lv == rv ? BigDecimal(1) : BigDecimal(0));
                if (node->val == "!=")
                    return RuntimeVal(lv != rv ? BigDecimal(1) : BigDecimal(0));
                if (node->val == "<")
                    return RuntimeVal(lv < rv ? BigDecimal(1) : BigDecimal(0));
                if (node->val == ">")
                    return RuntimeVal(lv > rv ? BigDecimal(1) : BigDecimal(0));
                if (node->val == "<=")
                    return RuntimeVal(lv <= rv ? BigDecimal(1) : BigDecimal(0));
                if (node->val == ">=")
                    return RuntimeVal(lv >= rv ? BigDecimal(1) : BigDecimal(0));
            }
            if (node->val == "+")
            {
                auto *lstr = lhs.as_str();
                auto *rstr = rhs.as_str();
                if (lstr && rstr)
                {
                    return RuntimeVal(lstr->value + rstr->value);
                }
            }
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << ln << "] Runtime error: binary operand type mismatch\n";
            Console::setColor(Console::Color::Reset);
            return RuntimeVal();
        }
        case ASTNode::IF:
        {
            EvalFrame cond_frame = EvalFrame::make_expr_frame();
            auto cond = eval(node->children[0].get(), scope, cond_frame);
            auto *condnum = cond.as_num();
            if (!condnum)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: if‑condition must be number\n";
                Console::setColor(Console::Color::Reset);
                break;
            }
            if (!(condnum->value == BigDecimal("0")))
            {
                eval(node->children[1].get(), scope, frame);
            }
            else if (node->children.size() >= 3)
            {
                eval(node->children[2].get(), scope, frame);
            }
            return RuntimeVal();
        }
        case ASTNode::WHILE:
        {
            for (;;)
            {
                EvalFrame cond_frame = EvalFrame::make_expr_frame();
                auto cond = eval(node->children[0].get(), scope, cond_frame);
                auto *condnum = cond.as_num();
                if (!condnum)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: while‑condition must be number\n";
                    Console::setColor(Console::Color::Reset);
                    break;
                }
                if (condnum->value == BigDecimal("0"))
                    break;

                eval(node->children[1].get(), scope, frame);

                if (frame.break_flag)
                {
                    frame.break_flag = false;
                    break;
                }
                if (frame.continue_flag)
                {
                    frame.continue_flag = false;
                    continue;
                }
                if (frame.has_return)
                {
                    return RuntimeVal();
                    break;
                }
            }
            return RuntimeVal();
        }
        case ASTNode::FOR_IN:
        {
            EvalFrame seq_frame = EvalFrame::make_expr_frame();
            auto seq_val = eval(node->children[0].get(), scope, seq_frame);
            auto *arrptr = seq_val.as_array();
            if (!arrptr)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: foreach expects array\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            Array &arr = arrptr->value;
            for (size_t idx = 0; idx < arr.size(); idx++)
            {
                Scope blk_scope(scope);
                blk_scope.set(node->val, arr[idx].clone());
                eval(node->children[1].get(), &blk_scope, frame);
                auto *modified = blk_scope.get(node->val);
                if (modified != nullptr)
                {
                    arr[idx] = std::move(*modified);
                }
                if (frame.break_flag)
                {
                    frame.break_flag = false;
                    break;
                }
                if (frame.continue_flag)
                {
                    frame.continue_flag = false;
                    continue;
                }
                if (frame.has_return)
                {
                    return RuntimeVal();
                    break;
                }
            }
            return RuntimeVal();
        }
        case ASTNode::SWITCH:
        {
            EvalFrame subject_frame = EvalFrame::make_expr_frame();
            RuntimeVal subject = eval(node->children[0].get(), scope, subject_frame);
            auto pattern_match = [&](auto &&self, ASTNode *pat, RuntimeVal &subj, Scope *sc, EvalFrame &ef) -> bool
            {
                if (pat->kind == ASTNode::BINARY)
                {
                    if (pat->val == "&&" || pat->val == "and")
                    {
                        auto lhsOk = self(self, pat->children[0].get(), subj, sc, ef);
                        auto rhsOk = self(self, pat->children[1].get(), subj, sc, ef);
                        return lhsOk && rhsOk;
                    }
                    if (pat->val == "||" || pat->val == "or")
                    {
                        auto lhsOk = self(self, pat->children[0].get(), subj, sc, ef);
                        if (lhsOk)
                            return true;
                        auto rhsOk = self(self, pat->children[1].get(), subj, sc, ef);
                        return rhsOk;
                    }
                }
                EvalFrame tmp = EvalFrame::make_expr_frame();
                RuntimeVal pv = eval(pat, sc, tmp);
                return (subj == pv);
            };

            bool branchFound = false;
            for (size_t ci = 1; ci < node->children.size(); ci++)
            {
                ASTNode *caseNode = node->children[ci].get();
                if (caseNode->kind != ASTNode::CASE_PATTERN)
                    continue;

                bool isDefault = (caseNode->val == "default" || caseNode->val == "zhumipingan");
                ASTNode *patternAst = caseNode->children[0].get();
                ASTNode *guardAst = caseNode->children[1].get();
                ASTNode *bodyAst = caseNode->children[2].get();

                bool match_ok = false;
                if (isDefault)
                {
                    match_ok = true;
                }
                else
                {
                    EvalFrame pef = EvalFrame::make_expr_frame();
                    match_ok = pattern_match(pattern_match, patternAst, subject, scope, pef);
                }
                if (!match_ok)
                    continue;
                EvalFrame guard_frame = EvalFrame::make_expr_frame();
                RuntimeVal guard_val = eval(guardAst, scope, guard_frame);
                auto *gnum = guard_val.as_num();
                if (!gnum || gnum->value == BigDecimal("0"))
                {
                    continue;
                }
                eval(bodyAst, scope, frame);
                branchFound = true;
                break;
            }
            (void)branchFound;
            return RuntimeVal();
        }
        case ASTNode::CASE_PATTERN:
        {
            return RuntimeVal();
        }
        case ASTNode::RANGE:
        {
            EvalFrame s_frame = EvalFrame::make_expr_frame();
            auto s = eval(node->children[0].get(), scope, s_frame);

            EvalFrame e_frame = EvalFrame::make_expr_frame();
            auto e = eval(node->children[1].get(), scope, e_frame);

            auto *s_num = s.as_num();
            auto *e_num = e.as_num();
            if (!s_num || !e_num)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: range() arguments must be number\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal(Array{});
            }
            BigDecimal start = s_num->value;
            BigDecimal end = e_num->value;
            BigDecimal step("1");
            if (node->children.size() >= 3)
            {
                EvalFrame st_frame = EvalFrame::make_expr_frame();
                auto st = eval(node->children[2].get(), scope, st_frame);
                auto *st_num = st.as_num();
                if (!st_num)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: range() step must be number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal(Array{});
                }
                step = st_num->value;
            }
            Array arr;
            BigDecimal cur = start;
            while (cur < end)
            {
                arr.push_back(RuntimeVal(cur));
                cur = cur + step;
            }
            return RuntimeVal(std::move(arr));
        }
        case ASTNode::ARRAY_LIT:
        {
            Array arr;
            for (auto &c : node->children)
            {
                EvalFrame elem_frame = EvalFrame::make_expr_frame();
                arr.push_back(eval(c.get(), scope, elem_frame));
            }
            return RuntimeVal(std::move(arr));
        }
        case ASTNode::DICT_LIT:
        {
            Dict d;
            for (size_t i = 0; i < node->children.size(); i += 2)
            {
                EvalFrame k_frame = EvalFrame::make_expr_frame();
                auto k = eval(node->children[i].get(), scope, k_frame);

                EvalFrame v_frame = EvalFrame::make_expr_frame();
                auto v = eval(node->children[i + 1].get(), scope, v_frame);

                auto *k_arr = k.as_array();
                auto *k_dict = k.as_dict();
                if (k_arr || k_dict)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: cannot use array/dict as dict key\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                d.insert_or_assign(std::move(k), std::move(v));
            }
            return RuntimeVal(std::move(d));
        }
        case ASTNode::LAMBDA_EXPR:
        {
            std::vector<FuncParamInfo> paramsInfo;
            size_t paramCnt = node->children.size() - 1;
            for (size_t i = 0; i < paramCnt; i++)
            {
                ASTNode *pd = node->children[i].get();
                FuncParamInfo pi;
                pi.name = pd->children[0]->val;
                if (pd->children.size() >= 2)
                {
                    pi.default_expr = pd->children[1].get();
                }
                paramsInfo.push_back(std::move(pi));
            }
            ASTNode *bodyAst = node->children.back().get();
            FuncT ft = std::make_pair(std::move(paramsInfo), bodyAst);
            return RuntimeVal(std::move(ft));
        }
        case ASTNode::INDEX:
        {
            auto base_p = scope->get(node->val);
            auto base = base_p->clone();

            EvalFrame idx_frame = EvalFrame::make_expr_frame();
            auto idxv = eval(node->children[0].get(), scope, idx_frame);

            auto *arrptr = base.as_array();
            if (arrptr)
            {
                auto *idxnum = idxv.as_num();
                if (!idxnum)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: array index must be number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                size_t idx;
                if (!safe_to_size_t(idxnum->value, idx))
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: invalid index value\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                if (idx >= arrptr->value.size())
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: array index out of bounds\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                return arrptr->value[idx].clone();
            }
            auto *dictptr = base.as_dict();
            if (dictptr)
            {
                auto it = map_find_const(dictptr->value, idxv);
                if (it == dictptr->value.end())
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: dict key not found\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                return it->second.clone();
            }
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << ln << "] Runtime error: index requires array/dict\n";
            Console::setColor(Console::Color::Reset);
            return RuntimeVal();
        }
        case ASTNode::MEMBER_ACCESS:
        {
            EvalFrame base_frame = EvalFrame::make_expr_frame();
            auto base_val = eval(node->children[0].get(), scope, base_frame);
            std::string mem = node->val;
            auto *obj_ptr = base_val.as_object();
            if (obj_ptr != nullptr)
            {
                auto &members = obj_ptr->value->members;
                auto it = members.find(mem);
                if (it != members.end())
                {
                    return it->second.clone();
                }
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: object member '" << mem << "' not found\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            auto *cls_ptr = base_val.as_classmeta();
            if (cls_ptr != nullptr)
            {
                ClassMeta *cm = cls_ptr->value.get();
                auto it = cm->static_methods.find(mem);
                if (it != cm->static_methods.end())
                {
                    return RuntimeVal(it->second);
                }
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: class static member " << mem << " not found\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << ln << "] Runtime error: . operator needs object/class\n";
            Console::setColor(Console::Color::Reset);
            return RuntimeVal();
        }
        case ASTNode::MEMBER_CALL:
        {
            std::string method_name = node->val;
            bool is_super_call = false;
            std::shared_ptr<ClassMeta> super_lookup_meta;
            RuntimeVal base_val;
            if (node->children[0]->kind == ASTNode::VAR && node->children[0]->val == "super")
            {
                is_super_call = true;
                auto p_super_meta = scope->get("__super_meta");
                auto p_self = scope->get("self");
                if (!p_super_meta || p_super_meta->as_classmeta() == nullptr || !p_self)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: super can only be used inside instance method\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                super_lookup_meta = p_super_meta->as_classmeta()->value;
                base_val = p_self->clone();
            }
            else
            {
                EvalFrame base_frame = EvalFrame::make_expr_frame();
                base_val = eval(node->children[0].get(), scope, base_frame);
            }
            auto *obj_ptr = base_val.as_object();
            if (obj_ptr != nullptr)
            {
                ObjectInstance &obj = *obj_ptr->value;
                ClassMeta *meta = obj.meta.get();
                FuncT *ft_ptr;
                if (is_super_call)
                {
                    ft_ptr = lookup_instance_method_from(super_lookup_meta.get(), method_name);
                }
                else
                {
                    ft_ptr = lookup_instance_method(meta, method_name);
                }
                if (!ft_ptr)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: no instance method " << method_name << "\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                FuncT &ft = *ft_ptr;
                Scope fscope(scope);
                fscope.set("self", base_val.clone());
                fscope.set("__super_meta", RuntimeVal(obj.meta->super_meta));

                size_t argCount = node->children.size() - 1;
                std::vector<FuncParamInfo> &params = ft.first;
                ASTNode *func_body = ft.second;

                int variadicIndex = -1;
                for (int pi = 0; pi < (int)params.size(); pi++)
                {
                    if (params[pi].name.substr(0, 3) == "...")
                    {
                        variadicIndex = pi;
                        break;
                    }
                }
                Array restArr;
                for (size_t argIdx = 0; argIdx < argCount; argIdx++)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto argVal = eval(node->children[argIdx + 1].get(), scope, arg_frame);
                    int paramIdx = static_cast<int>(argIdx) + 1;
                    if (variadicIndex != -1 && paramIdx >= variadicIndex)
                    {
                        restArr.push_back(std::move(argVal));
                    }
                    else
                    {
                        fscope.set(params[paramIdx].name, std::move(argVal));
                    }
                }
                for (int pi = 1; pi < (int)params.size(); pi++)
                {
                    FuncParamInfo &pinfo = params[pi];
                    if (pinfo.name.substr(0, 3) == "...")
                    {
                        std::string realName = pinfo.name.substr(3);
                        fscope.set(realName, RuntimeVal(std::move(restArr)));
                        continue;
                    }
                    int userArgCnt = static_cast<int>(argCount);
                    if (pi < userArgCnt + 1 && (variadicIndex == -1 || pi < variadicIndex))
                    {
                        continue;
                    }
                    if (pinfo.default_expr != nullptr)
                    {
                        EvalFrame def_frame = EvalFrame::make_expr_frame();
                        RuntimeVal defVal = eval(pinfo.default_expr, &fscope, def_frame);
                        fscope.set(pinfo.name, std::move(defVal));
                    }
                    else
                    {
                        Console::setColor(Console::Color::Red);
                        std::cerr << "[" << ln << "] Runtime error: method missing argument '" << pinfo.name << "' no default\n";
                        Console::setColor(Console::Color::Reset);
                        return RuntimeVal();
                    }
                }

                EvalFrame child_frame = frame.make_child();
                eval(func_body, &fscope, child_frame);
                return RuntimeVal(std::move(child_frame.ret_val));
            }
            auto *cls_ptr = base_val.as_classmeta();
            if (cls_ptr != nullptr)
            {
                ClassMeta *cm = cls_ptr->value.get();
                FuncT *ft_ptr = lookup_static_method(cm, method_name);
                if (!ft_ptr)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: no static method " << method_name << " on class\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                FuncT &ft = *ft_ptr;
                Scope fscope(scope);
                std::vector<FuncParamInfo> &params = ft.first;
                ASTNode *func_body = ft.second;

                size_t argCount = node->children.size() - 1;
                int variadicIndex = -1;
                for (int pi = 0; pi < (int)params.size(); pi++)
                {
                    if (params[pi].name.substr(0, 3) == "...")
                    {
                        variadicIndex = pi;
                        break;
                    }
                }
                Array restArr;
                for (size_t i = 0; i < argCount; i++)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto argVal = eval(node->children[i + 1].get(), scope, arg_frame);
                    if (variadicIndex != -1 && (int)i >= variadicIndex)
                    {
                        restArr.push_back(std::move(argVal));
                    }
                    else
                    {
                        fscope.set(params[i].name, std::move(argVal));
                    }
                }

                for (int pi = 0; pi < (int)params.size(); pi++)
                {
                    FuncParamInfo &pinfo = params[pi];
                    if (pinfo.name.substr(0, 3) == "...")
                    {
                        std::string realName = pinfo.name.substr(3);
                        fscope.set(realName, RuntimeVal(std::move(restArr)));
                        continue;
                    }
                    if ((size_t)pi < argCount && (variadicIndex == -1 || pi < (size_t)variadicIndex))
                    {
                        continue;
                    }
                    if (pinfo.default_expr != nullptr)
                    {
                        EvalFrame def_frame = EvalFrame::make_expr_frame();
                        RuntimeVal defVal = eval(pinfo.default_expr, &fscope, def_frame);
                        fscope.set(pinfo.name, std::move(defVal));
                    }
                    else
                    {
                        Console::setColor(Console::Color::Red);
                        std::cerr << "[" << ln << "] Runtime error: static method missing argument '" << pinfo.name << "' no default\n";
                        Console::setColor(Console::Color::Reset);
                        return RuntimeVal();
                    }
                }

                EvalFrame child_frame = frame.make_child();
                eval(func_body, &fscope, child_frame);
                return RuntimeVal(std::move(child_frame.ret_val));
            }
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << ln << "] Runtime error: member call requires object/class\n";
            Console::setColor(Console::Color::Reset);
            return RuntimeVal();
        }
        case ASTNode::CLASS_DEF:
        {
            auto meta = std::make_shared<ClassMeta>();
            meta->name = node->val;
            meta->super_class_name = node->val2;
            meta->super_meta = resolve_superclass(meta->super_class_name, scope, node->line);
            for (auto &child : node->children)
            {
                if (child->kind != ASTNode::FUNC_DEF)
                    continue;
                std::string tag = child->val;
                auto sep = tag.find('|');
                std::string fname = tag.substr(0, sep);
                std::string mode = tag.substr(sep + 1);
                FuncT ft;
                std::vector<FuncParamInfo> paramsInfo;
                size_t paramCnt = child->children.size() - 1;
                for (size_t i = 0; i < paramCnt; ++i)
                {
                    ASTNode *pd = child->children[i].get();
                    FuncParamInfo pi;
                    pi.name = pd->children[0]->val;
                    if (pd->children.size() >= 2)
                    {
                        pi.default_expr = pd->children[1].get();
                    }
                    paramsInfo.push_back(std::move(pi));
                }
                ASTNode *body = child->children.back().get();
                ft = std::make_pair(std::move(paramsInfo), body);
                if (mode == "instance")
                {
                    meta->instance_methods[fname] = ft;
                }
                else if (mode == "static")
                {
                    meta->static_methods[fname] = ft;
                }
            }
            scope->set(node->val, RuntimeVal(std::move(meta)));
            return RuntimeVal();
        }
        case ASTNode::NEW_OBJ:
        {
            RuntimeVal *cls_val_ptr = scope->get(node->val);
            if (!cls_val_ptr)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: new requires class\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            auto *cls_ptr = cls_val_ptr->as_classmeta();
            if (!cls_ptr)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: new requires class\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            std::shared_ptr<ClassMeta> meta_raw = cls_ptr->value;
            std::shared_ptr<ObjectInstance> obj_inst_ptr = std::make_shared<ObjectInstance>(meta_raw, std::unordered_map<std::string, RuntimeVal>{});
            RuntimeVal obj_val(obj_inst_ptr);
            auto it_init = obj_inst_ptr->meta->instance_methods.find("init");
            if (it_init != obj_inst_ptr->meta->instance_methods.end())
            {
                FuncT &init_ft = it_init->second;
                Scope fscope(scope);
                fscope.set("self", obj_val.clone());
                fscope.set("__super_meta", RuntimeVal(obj_inst_ptr->meta->super_meta));

                std::vector<FuncParamInfo> &params = init_ft.first;
                ASTNode *func_body = init_ft.second;
                size_t argCount = node->children.size();

                int variadicIndex = -1;
                for (int pi = 0; pi < (int)params.size(); pi++)
                {
                    if (params[pi].name.substr(0, 3) == "...")
                    {
                        variadicIndex = pi;
                        break;
                    }
                }
                Array restArr;

                for (size_t argIdx = 0; argIdx < argCount; argIdx++)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto argVal = eval(node->children[argIdx].get(), scope, arg_frame);
                    int paramIdx = static_cast<int>(argIdx) + 1;
                    if (variadicIndex != -1 && paramIdx >= variadicIndex)
                    {
                        restArr.push_back(std::move(argVal));
                    }
                    else
                    {
                        fscope.set(params[paramIdx].name, std::move(argVal));
                    }
                }

                for (int pi = 1; pi < (int)params.size(); pi++)
                {
                    FuncParamInfo &pinfo = params[pi];
                    if (pinfo.name.substr(0, 3) == "...")
                    {
                        std::string realName = pinfo.name.substr(3);
                        fscope.set(realName, RuntimeVal(std::move(restArr)));
                        continue;
                    }
                    int userArgCnt = static_cast<int>(argCount);
                    if (pi < userArgCnt + 1 && (variadicIndex == -1 || pi < variadicIndex))
                    {
                        continue;
                    }
                    if (pinfo.default_expr != nullptr)
                    {
                        EvalFrame def_frame = EvalFrame::make_expr_frame();
                        RuntimeVal defVal = eval(pinfo.default_expr, &fscope, def_frame);
                        fscope.set(pinfo.name, std::move(defVal));
                    }
                    else
                    {

                        Console::setColor(Console::Color::Red);
                        std::cerr << "[" << ln << "] Runtime error: constructor init missing argument '" << pinfo.name << "' no default\n";
                        Console::setColor(Console::Color::Reset);
                        return RuntimeVal();
                    }
                }

                EvalFrame subframe = frame.make_child();
                eval(func_body, &fscope, subframe);
            }
            return obj_val;
        }
        case ASTNode::FUNC_DEF:
        {
            std::vector<FuncParamInfo> paramsInfo;
            size_t paramCnt = node->children.size() - 1;
            for (size_t i = 0; i < paramCnt; i++)
            {
                ASTNode *pd = node->children[i].get();
                FuncParamInfo pi;
                pi.name = pd->children[0]->val;
                if (pd->children.size() >= 2)
                {
                    pi.default_expr = pd->children[1].get();
                }
                paramsInfo.push_back(std::move(pi));
            }
            ASTNode *body = node->children.back().get();
            scope->set(node->val, RuntimeVal(std::make_pair(std::move(paramsInfo), body)));
            return RuntimeVal();
        }
        case ASTNode::CALL:
        {
            size_t ln = node->line;
            if (node->val == "output")
            {
                for (auto &child : node->children)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto arg = eval(child.get(), scope, arg_frame);
                    if (arg.type() == RtKind::STRING)
                    {
                        std::cout << arg.as_str()->value;
                    }
                    else
                    {
                        std::cout << arg.to_string();
                    }
                }
                std::cout.flush();
                return RuntimeVal();
            }
            if (node->val == "outputLine")
            {
                for (auto &child : node->children)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto arg = eval(child.get(), scope, arg_frame);
                    if (arg.type() == RtKind::STRING)
                    {
                        std::cout << arg.as_str()->value;
                    }
                    else
                    {
                        std::cout << arg.to_string();
                    }
                }
                std::cout << "\n";
                std::cout.flush();
                return RuntimeVal();
            }
            if (node->val == "prminput")
            {
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                if (arg.type() == RtKind::STRING)
                {
                    std::cout << arg.as_str()->value;
                }
                else
                {
                    std::cout << arg.to_string();
                }
                std::cout.flush();
                std::string s;
                std::getline(std::cin, s);
                if (std::cin.fail())
                {
                    std::cin.clear();
                    Console::setColor(Console::Color::Red);
                    std::cerr << "Input Error!\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal("");
                }
                return RuntimeVal(s);
            }
            if (node->val == "input")
            {
                std::string s;
                std::getline(std::cin, s);
                if (std::cin.fail())
                {
                    std::cin.clear();
                    Console::setColor(Console::Color::Red);
                    std::cerr << "Input Error!\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal("");
                }
                return RuntimeVal(s);
            }
            if (node->val == "tonum")
            {
                if (node->children.size() != 1)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: tonum() expects exactly one argument\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                auto *sptr = arg.as_str();
                if (!sptr)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: tonum() argument must be string\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                BigDecimal tempNum;
                try
                {
                    tempNum = BigDecimal(sptr->value);
                }
                catch (...)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: tonum() invalid number string: " << sptr->value << "\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                return RuntimeVal(std::move(tempNum));
            }
            if (node->val == "tostring")
            {
                if (node->children.size() != 1)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: tostring() expects exactly one argument\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                return RuntimeVal(arg.to_string());
            }
            if (node->val == "time")
            {
                if (node->children.size() != 0)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: time() takes no arguments\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                auto now = std::chrono::system_clock::now();
                auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                return RuntimeVal(BigDecimal(std::to_string(sec)));
            }
            if (node->val == "time_ms")
            {
                if (node->children.size() != 0)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: time_ms() takes no arguments\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                return RuntimeVal(BigDecimal(std::to_string(ms)));
            }
            if (node->val == "sleep")
            {
                if (node->children.size() != 1)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: sleep() expects 1 argument(ms)\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                auto *numptr = arg.as_num();
                if (!numptr)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: sleep() argument must be number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                size_t ms;
                if (!safe_to_size_t(numptr->value, ms))
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: sleep() invalid millisecond value\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                return RuntimeVal();
            }
            if (node->val == "args")
            {
                if (!node->children.empty())
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: args() takes no arguments\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                Array arr;
                for (auto &s : this->cmd_args)
                {
                    arr.push_back(RuntimeVal(s));
                }
                return RuntimeVal(std::move(arr));
            }
            if (node->val == "conc")
            {
                if (node->children.size() != 2)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: conc() expects exactly two arguments\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                EvalFrame a_frame = EvalFrame::make_expr_frame();
                auto arg0 = eval(node->children[0].get(), scope, a_frame);
                EvalFrame b_frame = EvalFrame::make_expr_frame();
                auto arg1 = eval(node->children[1].get(), scope, b_frame);

                auto *arr0 = arg0.as_array();
                if (!arr0)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: conc() first argument must be array\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                Array res = arr0->value;
                auto *arr1 = arg1.as_array();
                if (arr1)
                {
                    for (auto &elem : arr1->value)
                    {
                        res.push_back(elem.clone());
                    }
                }
                else
                {
                    res.push_back(arg1.clone());
                }
                return RuntimeVal(std::move(res));
            }
            if (node->val == "len")
            {
                if (node->children.size() != 1)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: len() expects exactly one argument\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                if (auto *ap = arg.as_array())
                {
                    return RuntimeVal(BigDecimal(std::to_string(ap->value.size())));
                }
                else if (auto *sp = arg.as_str())
                {
                    return RuntimeVal(BigDecimal(std::to_string(sp->value.size())));
                }
                else
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: len() expects array or string\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
            }
            if (node->val == "pi")
            {
                if (!node->children.empty())
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: pi() takes no arguments\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                return RuntimeVal(BigDecimal("3.14159265358979323846264338327950288419716939937510"));
            }
            if (node->val == "sqrt")
            {
                if (node->children.size() != 1)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: sqrt() expects exactly one argument\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                auto *numptr = arg.as_num();
                if (!numptr)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: sqrt() argument must be number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                if (numptr->value.compare(BigDecimal("0")) < 0)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: sqrt() negative input\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                double d = std::stod(numptr->value.to_string());
                double res = std::sqrt(d);
                return RuntimeVal(BigDecimal(std::to_string(res)));
            }
            if (node->val == "abs")
            {
                if (node->children.size() != 1)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: abs() expects exactly one argument\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                auto *numptr = arg.as_num();
                if (!numptr)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: abs() argument must be number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                BigDecimal v = numptr->value;
                v.negative = false;
                return RuntimeVal(std::move(v));
            }
            if (node->val == "pow")
            {
                if (node->children.size() != 2)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: pow() expects exactly two arguments(base, exp)\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                EvalFrame a_frame = EvalFrame::make_expr_frame();
                auto arg0 = eval(node->children[0].get(), scope, a_frame);
                EvalFrame b_frame = EvalFrame::make_expr_frame();
                auto arg1 = eval(node->children[1].get(), scope, b_frame);
                auto *base_ptr = arg0.as_num();
                auto *exp_ptr = arg1.as_num();
                if (!base_ptr || !exp_ptr)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: pow() arguments must be number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                double base = std::stod(base_ptr->value.to_string());
                double exp = std::stod(exp_ptr->value.to_string());
                double res = std::pow(base, exp);
                return RuntimeVal(BigDecimal(std::to_string(res)));
            }
            if (node->val == "cbrt")
            {
                if (node->children.size() != 1)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: cbrt() expects exactly one argument\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                auto *numptr = arg.as_num();
                if (!numptr)
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: cbrt() argument must be number\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
                double d = std::stod(numptr->value.to_string());
                double res = std::cbrt(d);
                return RuntimeVal(BigDecimal(std::to_string(res)));
            }
            if (node->val == "eBuddha")
            {
                std::string eB = R"XXX(
Eastern mysticism:
                _ooOoo_
               o8888888o
               88" . "88
               (| -_- |)
               O\  =  /O
            ____/`---'\____
          .'  \\|     |//  `.
         /  \\|||  :  |||//  \
        /  _||||| -:- |||||-  \
        |   | \\\  -  /// |   |
        | \_|  ''\---/''  |   |
         \  .-\__ `-` ___/-. /
        ___`. .'  /--\  `. .'___
    ."" '<  `.___\_<|>_/___.' >' "".
   | |:`-`. `_. `\`.;`\ _ /`;.`/-`:| |
   \  \ `_.   \_ __\ /__ _/   .-` /  /
====`-.____`.___ \_____/___.-`___.-'=====
                  | | |
                 `=---='
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
          佛祖保佑       永无BUG
          用例全过       解析正确
          编译通过       运行正常
                ----------
                |********|
                |********|
                |********|
                ----------
                    )XXX";
                Console::setColor(Console::Color::Green);
                std::cout << eB << std::endl;
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            auto fv = scope->get(node->val)->clone();
            auto *fptr = fv.as_func();
            if (!fptr)
            {
                Console::setColor(Console::Color::Red);
                std::cerr << "[" << ln << "] Runtime error: not a function\n";
                Console::setColor(Console::Color::Reset);
                return RuntimeVal();
            }
            Scope fscope(scope);
            FuncT &ft = fptr->value;
            std::vector<FuncParamInfo> &params = ft.first;
            ASTNode *func_body = ft.second;

            size_t argCount = node->children.size();
            int variadicIndex = -1;
            for (int pi = 0; pi < (int)params.size(); pi++)
            {
                if (params[pi].name.substr(0, 3) == "...")
                {
                    variadicIndex = pi;
                    break;
                }
            }
            Array restArr;
            for (size_t i = 0; i < argCount; i++)
            {
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto argVal = eval(node->children[i].get(), scope, arg_frame);
                if (variadicIndex != -1 && (int)i >= variadicIndex)
                {
                    restArr.push_back(std::move(argVal));
                }
                else
                {
                    fscope.set(params[i].name, std::move(argVal));
                }
            }
            for (int pi = 0; pi < (int)params.size(); pi++)
            {
                FuncParamInfo &pinfo = params[pi];
                if (pinfo.name.substr(0, 3) == "...")
                {
                    std::string realName = pinfo.name.substr(3);
                    fscope.set(realName, RuntimeVal(std::move(restArr)));
                    continue;
                }
                if ((size_t)pi < argCount && (variadicIndex == -1 || pi < (size_t)variadicIndex))
                {
                    continue;
                }
                if (pinfo.default_expr != nullptr)
                {
                    EvalFrame def_frame = EvalFrame::make_expr_frame();
                    RuntimeVal defVal = eval(pinfo.default_expr, &fscope, def_frame);
                    fscope.set(pinfo.name, std::move(defVal));
                }
                else
                {
                    Console::setColor(Console::Color::Red);
                    std::cerr << "[" << ln << "] Runtime error: function missing argument for '" << pinfo.name << "' (no default value)\n";
                    Console::setColor(Console::Color::Reset);
                    return RuntimeVal();
                }
            }

            EvalFrame child_frame = frame.make_child();
            eval(func_body, &fscope, child_frame);
            return RuntimeVal(std::move(child_frame.ret_val));
        }
        case ASTNode::RETURN:
        {
            EvalFrame expr_frame = EvalFrame::make_expr_frame();
            frame.ret_val = eval(node->children[0].get(), scope, expr_frame);
            frame.has_return = true;
            return frame.ret_val.clone();
        }
        case ASTNode::BREAK:
            frame.break_flag = true;
            return RuntimeVal();
        case ASTNode::CONTINUE:
            frame.continue_flag = true;
            return RuntimeVal();
        default:
            Console::setColor(Console::Color::Red);
            std::cerr << "[" << ln << "] Runtime error: unknown AST node\n";
            Console::setColor(Console::Color::Reset);
            return RuntimeVal();
        }
    }
    catch (std::runtime_error &ex)
    {
        Console::setColor(Console::Color::Red);
        std::cerr << "[" << ln << "] Runtime exception: " << ex.what() << "\n";
        Console::setColor(Console::Color::Reset);
    }
    catch (...)
    {
        Console::setColor(Console::Color::Red);
        std::cerr << "[" << ln << "] Runtime unknown exception\n";
        Console::setColor(Console::Color::Reset);
    }
    return RuntimeVal();
}
