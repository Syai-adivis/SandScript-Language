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
            std::cerr << "[" << line << "] Runtime error: array index must be number\n";
            return nullptr;
        }
        size_t i;
        if (!safe_to_size_t(num_ptr->value, i))
        {
            std::cerr << "[" << line << "] Runtime error: invalid index value (must be non‑negative small integer)\n";
            return nullptr;
        }
        if (i >= arr_ptr->value.size())
        {
            std::cerr << "[" << line << "] Runtime error: array index out of bounds\n";
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
            std::cerr << "[" << line << "] Runtime error: dict key not found\n";
            return nullptr;
        }
        return &it->second;
    }
    std::cerr << "[" << line << "] Runtime error: cannot index‑assign non‑array/non‑dict\n";
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
        std::cerr << "[" << line << "] Runtime error: super class '" << super_name << "' not defined\n";
        return nullptr;
    }
    auto *super_meta_ptr = super_val->as_classmeta();
    if (!super_meta_ptr)
    {
        std::cerr << "[" << line << "] Runtime error: '" << super_name << "' is not a class\n";
        return nullptr;
    }
    return super_meta_ptr->value;
}

void Interpreter::import_file(const std::string &path)
{
    std::ifstream fin(path);
    if (!fin.is_open())
    {
        std::cerr << "Import error: cannot open file " << path << "\n";
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
                std::cerr << "[" << ln << "] Runtime error: undefined variable: " << node->val << "\n";
                return RuntimeVal();
            }
            return p->clone();
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
                std::cerr << "[" << ln << "] Runtime error: undefined variable: " << node->val << "\n";
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
            std::cerr << "[" << ln << "] Runtime error: member assign requires object instance\n";
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
                        std::cerr << "[" << ln << "] Runtime error: index invalid\n";
                        return RuntimeVal();
                    }
                    auto &lval = arr->value[i];
                    auto *lnum = lval.as_num();
                    if (!lnum)
                    {
                        std::cerr << "[" << ln << "] Runtime error: compound assign requires number\n";
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
                    std::cerr << "[" << ln << "] undefined var " << varname << "\n";
                    return RuntimeVal();
                }

                EvalFrame rhs_frame = EvalFrame::make_expr_frame();
                RuntimeVal rhs = eval(node->children[0].get(), scope, rhs_frame);

                auto *pv_num = pv->as_num();
                auto *rhs_num = rhs.as_num();
                if (!pv_num || !rhs_num)
                {
                    std::cerr << "[" << ln << "] Runtime error: compound assign requires number\n";
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
                    std::cerr << "[" << ln << "] Runtime error: index invalid for ++/--\n";
                    return RuntimeVal();
                }
                size_t i;
                if (!safe_to_size_t(idxnum->value, i) || i >= arr->value.size())
                {
                    std::cerr << "[" << ln << "] Runtime error: index invalid for ++/--\n";
                    return RuntimeVal();
                }
                RuntimeVal &lv = arr->value[i];
                auto *lv_num = lv.as_num();
                if (!lv_num)
                {
                    std::cerr << "[" << ln << "] Runtime error: ++/-- only for number\n";
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
                    std::cerr << "[" << ln << "] Runtime error: ++/-- only for number variable\n";
                    return RuntimeVal();
                }
                if (is_inc)
                    pv_num->value = pv_num->value + BigDecimal("1");
                else
                    pv_num->value = pv_num->value - BigDecimal("1");
                return RuntimeVal(pv_num->value);
            }
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
                    std::cerr << "[" << ln << "] Runtime error: not expects number\n";
                    return RuntimeVal();
                }
                return RuntimeVal((subnum->value == BigDecimal("0")) ? BigDecimal("1") : BigDecimal("0"));
            }
            else if (node->val == "-")
            {
                auto *subnum = sub.as_num();
                if (!subnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: unary minus expects number\n";
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
                    std::cerr << "[" << ln << "] Runtime error: and requires number\n";
                    return RuntimeVal();
                }
                if (lnum->value == BigDecimal("0"))
                    return RuntimeVal(BigDecimal("0"));

                EvalFrame rhs_frame = EvalFrame::make_expr_frame();
                auto rhs = eval(node->children[1].get(), scope, rhs_frame);
                auto *rnum = rhs.as_num();
                if (!rnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: and requires number\n";
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
                    std::cerr << "[" << ln << "] Runtime error: or requires number\n";
                    return RuntimeVal();
                }
                if (!(lnum->value == BigDecimal("0")))
                    return RuntimeVal(BigDecimal("1"));

                EvalFrame rhs_frame = EvalFrame::make_expr_frame();
                auto rhs = eval(node->children[1].get(), scope, rhs_frame);
                auto *rnum = rhs.as_num();
                if (!rnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: or requires number\n";
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
            std::cerr << "[" << ln << "] Runtime error: binary operand type mismatch\n";
            return RuntimeVal();
        }
        case ASTNode::IF:
        {
            // IF条件是表达式，使用expr‑frame
            EvalFrame cond_frame = EvalFrame::make_expr_frame();
            auto cond = eval(node->children[0].get(), scope, cond_frame);
            auto *condnum = cond.as_num();
            if (!condnum)
            {
                std::cerr << "[" << ln << "] Runtime error: if‑condition must be number\n";
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
                // while条件：表达式临时帧
                EvalFrame cond_frame = EvalFrame::make_expr_frame();
                auto cond = eval(node->children[0].get(), scope, cond_frame);
                auto *condnum = cond.as_num();
                if (!condnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: while‑condition must be number\n";
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
                std::cerr << "[" << ln << "] Runtime error: foreach expects array\n";
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
                std::cerr << "[" << ln << "] Runtime error: range() arguments must be number\n";
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
                    std::cerr << "[" << ln << "] Runtime error: range() step must be number\n";
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
                    std::cerr << "[" << ln << "] Runtime error: cannot use array/dict as dict key\n";
                    return RuntimeVal();
                }
                d.insert_or_assign(std::move(k), std::move(v));
            }
            return RuntimeVal(std::move(d));
        }
        case ASTNode::LAMBDA_EXPR:
        {
            std::vector<std::string> params;
            size_t paramCnt = node->children.size() - 1;
            for (size_t i = 0; i < paramCnt; i++)
            {
                params.push_back(node->children[i]->val);
            }
            ASTNode *bodyAst = node->children.back().get();
            FuncT ft = {params, bodyAst};
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
                    std::cerr << "[" << ln << "] Runtime error: array index must be number\n";
                    return RuntimeVal();
                }
                size_t idx;
                if (!safe_to_size_t(idxnum->value, idx))
                {
                    std::cerr << "[" << ln << "] Runtime error: invalid index value\n";
                    return RuntimeVal();
                }
                if (idx >= arrptr->value.size())
                {
                    std::cerr << "[" << ln << "] Runtime error: array index out of bounds\n";
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
                    std::cerr << "[" << ln << "] Runtime error: dict key not found\n";
                    return RuntimeVal();
                }
                return it->second.clone();
            }
            std::cerr << "[" << ln << "] Runtime error: index requires array/dict\n";
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
                std::cerr << "[" << ln << "] Runtime error: object member '" << mem << "' not found\n";
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
                std::cerr << "[" << ln << "] Runtime error: class static member " << mem << " not found\n";
                return RuntimeVal();
            }
            std::cerr << "[" << ln << "] Runtime error: . operator needs object/class\n";
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
                    std::cerr << "[" << ln << "] Runtime error: super can only be used inside instance method\n";
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
                    std::cerr << "[" << ln << "] Runtime error: no instance method " << method_name << "\n";
                    return RuntimeVal();
                }
                FuncT &ft = *ft_ptr;
                Scope fscope(scope);
                fscope.set("self", base_val.clone());
                fscope.set("__super_meta", RuntimeVal(obj.meta->super_meta));
                size_t argCount = node->children.size() - 1;
                for (size_t i = 0; i < argCount; i++)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto arg = eval(node->children[i + 1].get(), scope, arg_frame);
                    size_t paramIdx = i + 1;
                    if (paramIdx < ft.first.size())
                    {
                        fscope.set(ft.first[paramIdx], std::move(arg));
                    }
                }
                EvalFrame child_frame = frame.make_child();
                eval(ft.second, &fscope, child_frame);
                return RuntimeVal(std::move(child_frame.ret_val));
            }
            auto *cls_ptr = base_val.as_classmeta();
            if (cls_ptr != nullptr)
            {
                ClassMeta *cm = cls_ptr->value.get();
                FuncT *ft_ptr = lookup_static_method(cm, method_name);
                if (!ft_ptr)
                {
                    std::cerr << "[" << ln << "] Runtime error: no static method " << method_name << " on class\n";
                    return RuntimeVal();
                }
                FuncT &ft = *ft_ptr;
                Scope fscope(scope);
                size_t argCount = node->children.size() - 1;
                for (size_t i = 0; i < argCount; i++)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto arg = eval(node->children[i + 1].get(), scope, arg_frame);
                    if (i < ft.first.size())
                        fscope.set(ft.first[i], std::move(arg));
                }
                EvalFrame child_frame = frame.make_child();
                eval(ft.second, &fscope, child_frame);
                return RuntimeVal(std::move(child_frame.ret_val));
            }
            std::cerr << "[" << ln << "] Runtime error: member call requires object/class\n";
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
                std::vector<std::string> params;
                size_t paramCnt = child->children.size() - 1;
                for (size_t i = 0; i < paramCnt; i++)
                    params.push_back(child->children[i]->val);
                auto body = child->children.back().get();
                ft = {params, body};
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
                std::cerr << "[" << ln << "] Runtime error: new requires class\n";
                return RuntimeVal();
            }
            auto *cls_ptr = cls_val_ptr->as_classmeta();
            if (!cls_ptr)
            {
                std::cerr << "[" << ln << "] Runtime error: new requires class\n";
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
                for (size_t argi = 0; argi < node->children.size(); argi++)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto arg = eval(node->children[argi].get(), scope, arg_frame);
                    size_t param_idx = argi + 1;
                    if (param_idx < init_ft.first.size())
                    {
                        fscope.set(init_ft.first[param_idx], std::move(arg));
                    }
                }
                EvalFrame subframe = frame.make_child();
                eval(init_ft.second, &fscope, subframe);
            }
            return obj_val;
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
            size_t ln = node->line;
            // 内置函数
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
                    std::cerr << "Input Error!\n";
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
                    std::cerr << "Input Error!\n";
                    return RuntimeVal("");
                }
                return RuntimeVal(s);
            }
            if (node->val == "tonum")
            {
                if (node->children.size() != 1)
                {
                    std::cerr << "[" << ln << "] Runtime error: tonum() expects exactly one argument\n";
                    return RuntimeVal();
                }
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                auto *sptr = arg.as_str();
                if (!sptr)
                {
                    std::cerr << "[" << ln << "] Runtime error: tonum() argument must be string\n";
                    return RuntimeVal();
                }
                BigDecimal tempNum;
                try
                {
                    tempNum = BigDecimal(sptr->value);
                }
                catch (...)
                {
                    std::cerr << "[" << ln << "] Runtime error: tonum() invalid number string: " << sptr->value << "\n";
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
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                return RuntimeVal(arg.to_string());
            }
            if (node->val == "time")
            {
                if (node->children.size() != 0)
                {
                    std::cerr << "[" << ln << "] Runtime error: time() takes no arguments\n";
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
                    std::cerr << "[" << ln << "] Runtime error: time_ms() takes no arguments\n";
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
                    std::cerr << "[" << ln << "] Runtime error: sleep() expects 1 argument(ms)\n";
                    return RuntimeVal();
                }
                EvalFrame arg_frame = EvalFrame::make_expr_frame();
                auto arg = eval(node->children[0].get(), scope, arg_frame);
                auto *numptr = arg.as_num();
                if (!numptr)
                {
                    std::cerr << "[" << ln << "] Runtime error: sleep() argument must be number\n";
                    return RuntimeVal();
                }
                size_t ms;
                if (!safe_to_size_t(numptr->value, ms))
                {
                    std::cerr << "[" << ln << "] Runtime error: sleep() invalid millisecond value\n";
                    return RuntimeVal();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                return RuntimeVal();
            }
            if (node->val == "args")
            {
                if (!node->children.empty())
                {
                    std::cerr << "[" << ln << "] Runtime error: args() takes no arguments\n";
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
                    std::cerr << "[" << ln << "] Runtime error: conc() expects exactly two arguments\n";
                    return RuntimeVal();
                }
                EvalFrame a_frame = EvalFrame::make_expr_frame();
                auto arg0 = eval(node->children[0].get(), scope, a_frame);
                EvalFrame b_frame = EvalFrame::make_expr_frame();
                auto arg1 = eval(node->children[1].get(), scope, b_frame);

                auto *arr0 = arg0.as_array();
                if (!arr0)
                {
                    std::cerr << "[" << ln << "] Runtime error: conc() first argument must be array\n";
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
                    std::cerr << "[" << ln << "] Runtime error: len() expects exactly one argument\n";
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
                    std::cerr << "[" << ln << "] Runtime error: len() expects array or string\n";
                    return RuntimeVal();
                }
            }
            auto fv = scope->get(node->val)->clone();
            auto *fptr = fv.as_func();
            if (!fptr)
            {
                std::cerr << "[" << ln << "] Runtime error: not a function\n";
                return RuntimeVal();
            }

            Scope fscope(scope);
            auto &params = fptr->value.first;
            auto *func_body = fptr->value.second;
            size_t argCount = node->children.size();
            int variadicIndex = -1;
            for (int pi = 0; pi < (int)params.size(); pi++)
            {
                if (params[pi].substr(0, 3) == "...")
                {
                    variadicIndex = pi;
                    break;
                }
            }
            if (variadicIndex == -1)
            {
                if (argCount != params.size())
                {
                    std::cerr << "[" << ln << "] Runtime error: function expects " << params.size() << " arguments, got " << argCount << "\n";
                    return RuntimeVal();
                }
                for (size_t i = 0; i < params.size(); i++)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto arg = eval(node->children[i].get(), scope, arg_frame);
                    fscope.set(params[i], std::move(arg));
                }
            }
            else
            {
                size_t fixedCnt = (size_t)variadicIndex;
                Array restArr;
                for (size_t i = 0; i < argCount; i++)
                {
                    EvalFrame arg_frame = EvalFrame::make_expr_frame();
                    auto arg = eval(node->children[i].get(), scope, arg_frame);
                    if (i < fixedCnt)
                    {
                        fscope.set(params[i], std::move(arg));
                    }
                    else
                    {
                        restArr.push_back(std::move(arg));
                    }
                }
                for (size_t i = argCount; i < fixedCnt; i++)
                {
                    fscope.set(params[i], RuntimeVal());
                }
                std::string restName = params[variadicIndex].substr(3);
                fscope.set(restName, RuntimeVal(std::move(restArr)));
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
