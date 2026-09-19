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

struct Interpreter
{
    Scope global;
    RuntimeVal ret_val;
    bool has_return = false;
    bool break_flag = false;
    bool continue_flag = false;

    RuntimeVal *get_lvalue(RuntimeVal &root, ASTNode *idx_node, Scope *scope, size_t line);
    void import_file(const std::string &path);
    RuntimeVal eval(ASTNode *node, Scope *scope);
};

RuntimeVal *Interpreter::get_lvalue(RuntimeVal &root, ASTNode *idx_node, Scope *scope, size_t line)
{
    auto *arr_ptr = root.as_array();
    if (arr_ptr != nullptr)
    {
        auto idxv = eval(idx_node->children[0].get(), scope);
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
        auto idxv = eval(idx_node->children[0].get(), scope);
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
    eval(ast.get(), &global);
}

RuntimeVal Interpreter::eval(ASTNode *node, Scope *scope)
{
    if (has_return)
        return RuntimeVal(std::move(ret_val));
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
            auto v = eval(node->children[0].get(), scope);
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
            RuntimeVal *lv = get_lvalue(*base_p, node->children[0].get(), scope, ln);
            if (!lv)
                return RuntimeVal();
            auto new_val = eval(node->children[1].get(), scope);
            *lv = std::move(new_val);
            return RuntimeVal();
        }
        case ASTNode::MEMBER_ASSIGN:
        {
            auto base_val = eval(node->children[0].get(), scope);
            std::string mem_name = node->val;
            auto rhs_val = eval(node->children[1].get(), scope);
            auto *obj_ptr = base_val.as_object();
            if (obj_ptr != nullptr)
            {
                obj_ptr->value.members[mem_name] = std::move(rhs_val);
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
                RuntimeVal base = eval(baseNode.get(), scope);
                RuntimeVal idxVal = eval(idxNode.get(), scope);
                RuntimeVal rhsVal = eval(rhsNode.get(), scope);

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
                RuntimeVal rhs = eval(node->children[0].get(), scope);
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
                RuntimeVal base = eval(baseNode.get(), scope);
                RuntimeVal idxVal = eval(idxNode.get(), scope);
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
            auto sub = eval(node->children[0].get(), scope);
            if (node->val == "!")
            {
                auto *subnum = sub.as_num();
                if (!subnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: ! expects number\n";
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
            if (node->val == "&&")
            {
                auto lhs = eval(node->children[0].get(), scope);
                auto *lnum = lhs.as_num();
                if (!lnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: && requires number\n";
                    return RuntimeVal();
                }
                if (lnum->value == BigDecimal("0"))
                    return RuntimeVal(BigDecimal("0"));
                auto rhs = eval(node->children[1].get(), scope);
                auto *rnum = rhs.as_num();
                if (!rnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: && requires number\n";
                    return RuntimeVal();
                }
                return RuntimeVal((rnum->value == BigDecimal("0")) ? BigDecimal("0") : BigDecimal("1"));
            }
            if (node->val == "||")
            {
                auto lhs = eval(node->children[0].get(), scope);
                auto *lnum = lhs.as_num();
                if (!lnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: || requires number\n";
                    return RuntimeVal();
                }
                if (!(lnum->value == BigDecimal("0")))
                    return RuntimeVal(BigDecimal("1"));
                auto rhs = eval(node->children[1].get(), scope);
                auto *rnum = rhs.as_num();
                if (!rnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: || requires number\n";
                    return RuntimeVal();
                }
                return RuntimeVal((rnum->value == BigDecimal("0")) ? BigDecimal("0") : BigDecimal("1"));
            }

            auto lhs = eval(node->children[0].get(), scope);
            auto rhs = eval(node->children[1].get(), scope);
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
            auto cond = eval(node->children[0].get(), scope);
            auto *condnum = cond.as_num();
            if (!condnum)
            {
                std::cerr << "[" << ln << "] Runtime error: if‑condition must be number\n";
                break;
            }
            if (!(condnum->value == BigDecimal("0")))
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
            for (;;)
            {
                auto cond = eval(node->children[0].get(), scope);
                auto *condnum = cond.as_num();
                if (!condnum)
                {
                    std::cerr << "[" << ln << "] Runtime error: while‑condition must be number\n";
                    break;
                }
                if (condnum->value == BigDecimal("0"))
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

                eval(node->children[1].get(), &blk_scope);
                auto *modified = blk_scope.get(node->val);
                if (modified != nullptr)
                {
                    arr[idx] = std::move(*modified);
                }
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
                auto st = eval(node->children[2].get(), scope);
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
                arr.push_back(eval(c.get(), scope));
            return RuntimeVal(std::move(arr));
        }
        case ASTNode::DICT_LIT:
        {
            Dict d;
            for (size_t i = 0; i < node->children.size(); i += 2)
            {
                auto k = eval(node->children[i].get(), scope);
                auto v = eval(node->children[i + 1].get(), scope);
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
        case ASTNode::INDEX:
        {
            auto base_p = scope->get(node->val);
            auto base = RuntimeVal(std::move(*base_p));
            auto idxv = eval(node->children[0].get(), scope);
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
            auto base_val = eval(node->children[0].get(), scope);
            std::string mem = node->val;
            auto *obj_ptr = base_val.as_object();
            if (obj_ptr != nullptr)
            {
                auto &members = obj_ptr->value.members;
                auto it = members.find(mem);
                if (it != members.end())
                {
                    return RuntimeVal(std::move(it->second));
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
            auto base_val = eval(node->children[0].get(), scope);
            std::string method_name = node->val;
            auto *obj_ptr = base_val.as_object();
            if (obj_ptr != nullptr)
            {
                ObjectInstance &obj = obj_ptr->value;
                ClassMeta *meta = obj.meta.get();
                auto it = meta->instance_methods.find(method_name);
                if (it == meta->instance_methods.end())
                {
                    std::cerr << "[" << ln << "] Runtime error: no instance method " << method_name << "\n";
                    return RuntimeVal();
                }
                FuncT &ft = it->second;
                Scope fscope(scope);
                fscope.set("self", std::move(base_val));
                size_t argCount = node->children.size() - 1;
                for (size_t i = 0; i < argCount; i++)
                {
                    auto arg = eval(node->children[i + 1].get(), scope);
                    size_t paramIdx = i + 1;
                    if (paramIdx < ft.first.size())
                    {
                        fscope.set(ft.first[paramIdx], std::move(arg));
                    }
                }
                has_return = false;
                ret_val = RuntimeVal();
                eval(ft.second, &fscope);
                return RuntimeVal(std::move(ret_val));
            }
            auto *cls_ptr = base_val.as_classmeta();
            if (cls_ptr != nullptr)
            {
                ClassMeta *cm = cls_ptr->value.get();
                auto it = cm->static_methods.find(method_name);
                if (it == cm->static_methods.end())
                {
                    std::cerr << "[" << ln << "] Runtime error: no static method " << method_name << " on class\n";
                    return RuntimeVal();
                }
                FuncT &ft = it->second;
                Scope fscope(scope);
                size_t argCount = node->children.size() - 1;
                for (size_t i = 0; i < argCount; i++)
                {
                    auto arg = eval(node->children[i + 1].get(), scope);
                    if (i < ft.first.size())
                        fscope.set(ft.first[i], std::move(arg));
                }
                has_return = false;
                ret_val = RuntimeVal();
                eval(ft.second, &fscope);
                return RuntimeVal(std::move(ret_val));
            }
            std::cerr << "[" << ln << "] Runtime error: member call requires object/class\n";
            return RuntimeVal();
        }
        case ASTNode::CLASS_DEF:
        {
            auto meta = std::make_shared<ClassMeta>();
            meta->name = node->val;
            meta->super_class_name = node->val2;
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
            ObjectInstance obj(std::move(meta_raw), {});
            RuntimeVal obj_val(std::move(obj));

            auto it_init = obj_val.as_object()->value.meta->instance_methods.find("init");
            if (it_init != obj_val.as_object()->value.meta->instance_methods.end())
            {
                FuncT &init_ft = it_init->second;
                Scope fscope(scope);
                fscope.set("self", std::move(obj_val));
                for (size_t argi = 0; argi < node->children.size(); argi++)
                {
                    auto arg = eval(node->children[argi].get(), scope);
                    size_t param_idx = argi + 1;
                    if (param_idx < init_ft.first.size())
                    {
                        fscope.set(init_ft.first[param_idx], std::move(arg));
                    }
                }
                has_return = false;
                ret_val = RuntimeVal();
                eval(init_ft.second, &fscope);
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
            if (node->val == "output")
            {
                auto arg = eval(node->children[0].get(), scope);
                if (arg.type() == RtKind::STRING)
                {
                    std::cout << arg.as_str()->value;
                }
                else
                {
                    std::cout << arg.to_string();
                }
                std::cout.flush();
                return RuntimeVal();
            }
            if (node->val == "outputln")
            {
                auto arg = eval(node->children[0].get(), scope);
                if (arg.type() == RtKind::STRING)
                {
                    std::cout << arg.as_str()->value << "\n";
                }
                else
                {
                    std::cout << arg.to_string() << "\n";
                }
                std::cout.flush();
                return RuntimeVal();
            }
            if (node->val == "prminput")
            {
                auto arg = eval(node->children[0].get(), scope);
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
                auto arg = eval(node->children[0].get(), scope);
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
                auto arg = eval(node->children[0].get(), scope);
                return RuntimeVal(arg.to_string());
            }

            auto fv = RuntimeVal(std::move(*scope->get(node->val)));
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
                    auto arg = eval(node->children[i].get(), scope);
                    fscope.set(params[i], std::move(arg));
                }
            }
            else
            {
                size_t fixedCnt = (size_t)variadicIndex;
                Array restArr;
                for (size_t i = 0; i < argCount; i++)
                {
                    auto arg = eval(node->children[i].get(), scope);
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
            has_return = false;
            ret_val = RuntimeVal();
            eval(func_body, &fscope);
            return RuntimeVal(std::move(ret_val));
        }
        case ASTNode::RETURN:
        {
            ret_val = eval(node->children[0].get(), scope);
            has_return = true;
            return RuntimeVal(std::move(ret_val));
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
