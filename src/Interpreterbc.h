#include "Interpreterbch.h"

namespace
{
    std::shared_ptr<ClassMeta> get_closure_wrapper_meta()
    {
        static std::shared_ptr<ClassMeta> meta;
        if (!meta)
        {
            meta = std::make_shared<ClassMeta>();
            meta->name = "__VMClosureWrapper";
        }
        return meta;
    }
    static std::unordered_map<uint64_t, std::shared_ptr<VMClosure>> closure_pool_global;
}

RuntimeVal VM::wrap_closure(VMClosure clos)
{
    auto inst = std::make_shared<ObjectInstance>(get_closure_wrapper_meta(), std::unordered_map<std::string, RuntimeVal>{});
    std::shared_ptr<VMClosure> heap_clos = std::make_shared<VMClosure>(std::move(clos));
    uint64_t raw = reinterpret_cast<uint64_t>(heap_clos.get());
    inst->members["__closure_ptr"] = RuntimeVal(std::to_string(raw));
    closure_pool_global[raw] = std::move(heap_clos);
    return RuntimeVal(inst);
}

VMClosure *VM::unwrap_closure(RuntimeVal &v)
{
    auto *objv = v.as_object();
    if (!objv)
        return nullptr;
    auto &members = objv->value->members;
    auto it = members.find("__closure_ptr");
    if (it == members.end())
        return nullptr;
    auto *strv = it->second.as_str();
    if (!strv)
        return nullptr;
    uint64_t addr;
    try
    {
        addr = std::stoull(strv->value);
    }
    catch (...)
    {
        return nullptr;
    }
    auto pit = closure_pool_global.find(addr);
    if (pit == closure_pool_global.end())
        return nullptr;
    return pit->second.get();
}

RuntimeVal *Interpreter::get_lvalue(RuntimeVal &root, ASTNode *idx_node, Scope *scope, EvalFrame &expr_frame, size_t line)
{
    auto *arr_ptr = root.as_array();
    if (arr_ptr != nullptr)
    {
        EvalFrame dummy;
        auto idxv = RuntimeVal();
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
        EvalFrame dummy;
        RuntimeVal idxv;
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

FuncT *Interpreter::lookup_instance_method(ClassMeta *start_meta, const std::string &name)
{
    if (!start_meta)
        return nullptr;
    auto it = start_meta->instance_methods.find(name);
    if (it != start_meta->instance_methods.end())
    {
        return &it->second;
    }
    if (start_meta->super_meta)
    {
        return lookup_instance_method(start_meta->super_meta.get(), name);
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
    Compiler comp;
    comp.compile(ast.get());
    VM vm;
    EvalFrame dummy_frame;
    vm.run(comp.chunk, &global, this);
}

size_t Compiler::emit_jmp(OpCode op)
{
    chunk.emit_op(op);
    size_t pos = chunk.pc();
    chunk.emit_i32(0);
    return pos;
}

void Compiler::patch_jmp(size_t patch_pos)
{
    const size_t after_read = patch_pos + 4;
    int32_t offset = static_cast<int32_t>(chunk.pc() - after_read);
    chunk.patch_i32(patch_pos, offset);
}

void Compiler::patch_jmp(size_t patch_pos, size_t target_pc)
{
    const size_t after_read = patch_pos + 4;
    int32_t offset = static_cast<int32_t>(target_pc - after_read);
    chunk.patch_i32(patch_pos, offset);
}

VMClosure Compiler::compile_function(ASTNode *funcNode)
{
    VMClosure clos;
    Compiler subcomp;
    size_t paramCnt = funcNode->children.size() - 1;
    for (size_t i = 0; i < paramCnt; i++)
    {
        std::string pname = funcNode->children[i]->val;
        if (pname.substr(0, 3) == "...")
        {
            clos.variadicIndex = static_cast<int>(clos.params.size());
            pname = pname.substr(3);
        }
        subcomp.chunk.add_string(pname);
        clos.params.push_back(pname);
    }
    ASTNode *body = funcNode->children.back().get();
    subcomp.compile_stmt(body);

    uint32_t nilIdx = subcomp.chunk.add_const(RuntimeVal());
    subcomp.chunk.emit_op(OP_PUSH_CONST);
    subcomp.chunk.emit_u32(nilIdx);
    subcomp.chunk.emit_op(OP_RETURN);

    clos.chunk = std::move(subcomp.chunk);
    return clos;
}

void Compiler::compile(ASTNode *node)
{
    compile_stmt(node);
    chunk.emit_op(OP_HALT);
}

void Compiler::compile_stmt(ASTNode *node)
{
    if (!node)
        return;
    switch (node->kind)
    {
    case ASTNode::PROGRAM:
        for (auto &c : node->children)
            compile_stmt(c.get());
        break;
    case ASTNode::IMPORT:
    {
        uint32_t idx = chunk.add_string(node->val);
        chunk.emit_op(OP_IMPORT);
        chunk.emit_u32(idx);
        break;
    }
    case ASTNode::FUNC_DEF:
    {
        VMClosure clos = compile_function(node);
        RuntimeVal closureObj = VM::wrap_closure(std::move(clos));
        uint32_t cidx = chunk.add_const(std::move(closureObj));
        uint32_t sidx = chunk.add_string(node->val);
        chunk.emit_op(OP_PUSH_CONST);
        chunk.emit_u32(cidx);
        chunk.emit_op(OP_STORE_VAR);
        chunk.emit_u32(sidx);
        break;
    }
    case ASTNode::CLASS_DEF:
    {
        uint32_t nameIdx = chunk.add_string(node->val);
        uint32_t superIdx = chunk.add_string(node->val2);
        uint32_t methodCount = static_cast<uint32_t>(node->children.size());
        for (auto &child : node->children)
        {
            if (child->kind != ASTNode::FUNC_DEF)
                continue;
            uint64_t ptr = reinterpret_cast<uint64_t>(child.get());
            chunk.add_const(RuntimeVal(std::to_string(ptr)));
        }
        chunk.emit_op(OP_CLASS_META);
        chunk.emit_u32(nameIdx);
        chunk.emit_u32(superIdx);
        chunk.emit_u32(methodCount);
        uint32_t storeIdx = chunk.add_string(node->val);
        chunk.emit_op(OP_STORE_VAR);
        chunk.emit_u32(storeIdx);
        break;
    }
    case ASTNode::ASSIGN:
    {
        compile_expr(node->children[0].get());
        uint32_t idx = chunk.add_string(node->val);
        chunk.emit_op(OP_STORE_VAR);
        chunk.emit_u32(idx);
        break;
    }
    case ASTNode::INDEX_ASSIGN:
    {
        compile_expr(node->children[1].get());
        compile_expr(node->children[0].get());
        uint32_t nameIdx = chunk.add_string(node->val);
        chunk.emit_op(OP_LOAD_VAR);
        chunk.emit_u32(nameIdx);
        chunk.emit_op(OP_STORE_INDEX);
        break;
    }
    case ASTNode::MEMBER_ASSIGN:
    {
        compile_expr(node->children[1].get());
        compile_expr(node->children[0].get());
        uint32_t memIdx = chunk.add_string(node->val);
        chunk.emit_op(OP_STORE_MEMBER);
        chunk.emit_u32(memIdx);
        break;
    }
    case ASTNode::COMPOUND_ASSIGN:
    {
        std::string full = node->val;
        size_t colon = full.find(':');
        std::string op = full.substr(colon + 1);
        if (full.substr(0, 5) == "INDEX")
        {
            compile_expr(node->children[1].get());
            compile_expr(node->children[0].get());
            uint32_t nameIdx = chunk.add_string(full.substr(6));
            chunk.emit_op(OP_LOAD_VAR);
            chunk.emit_u32(nameIdx);
            if (op == "+=")
                chunk.emit_op(OP_COMPOUND_ADD);
            else if (op == "-=")
                chunk.emit_op(OP_COMPOUND_SUB);
            else if (op == "*=")
                chunk.emit_op(OP_COMPOUND_MUL);
            else if (op == "/=")
                chunk.emit_op(OP_COMPOUND_DIV);
        }
        else
        {
            std::string varname = full.substr(0, colon);
            compile_expr(node->children[0].get());
            uint32_t vidx = chunk.add_string(varname);
            chunk.emit_op(OP_LOAD_VAR);
            chunk.emit_u32(vidx);
            if (op == "+=")
                chunk.emit_op(OP_COMPOUND_ADD);
            else if (op == "-=")
                chunk.emit_op(OP_COMPOUND_SUB);
            else if (op == "*=")
                chunk.emit_op(OP_COMPOUND_MUL);
            else if (op == "/=")
                chunk.emit_op(OP_COMPOUND_DIV);
            chunk.emit_op(OP_STORE_VAR);
            chunk.emit_u32(vidx);
        }
        break;
    }
    case ASTNode::INCDEC:
    {
        std::string v = node->val;
        if (v.substr(0, 5) == "INDEX")
        {
            compile_expr(node->children[0].get());
            uint32_t baseIdx = chunk.add_string(node->children[1]->val);
            chunk.emit_op(OP_LOAD_VAR);
            chunk.emit_u32(baseIdx);
            if (v.find("++") != std::string::npos)
                chunk.emit_op(OP_INC);
            else
                chunk.emit_op(OP_DEC);
        }
        else
        {
            std::string varname = v.substr(0, v.size() - 2);
            uint32_t vidx = chunk.add_string(varname);
            chunk.emit_op(OP_LOAD_VAR);
            chunk.emit_u32(vidx);
            if (v.find("++") != std::string::npos)
                chunk.emit_op(OP_INC);
            else
                chunk.emit_op(OP_DEC);
            chunk.emit_op(OP_STORE_VAR);
            chunk.emit_u32(vidx);
        }
        break;
    }
    case ASTNode::IF:
    {
        compile_expr(node->children[0].get());
        size_t jmp_false_pos = emit_jmp(OP_JMP_IF_FALSE);
        compile_stmt(node->children[1].get());
        size_t jmp_end_pos = emit_jmp(OP_JMP);
        patch_jmp(jmp_false_pos);
        if (node->children.size() >= 3)
            compile_stmt(node->children[2].get());
        patch_jmp(jmp_end_pos);
        break;
    }
    case ASTNode::SWITCH:
    {
        auto *subject = node->children[0].get();
        compile_expr(subject); // stack: subject
        size_t caseCount = node->children.size() - 1;
        std::vector<size_t> case_jmp_false_pos;
        size_t switch_exit_jmp_pos = emit_jmp(OP_JMP);
        for (size_t ci = 1; ci < node->children.size(); ci++)
        {
            auto *caseNode = node->children[ci].get();
            // caseNode -> CASE_PATTERN
            // children[0] : pattern(nullptr for default)
            // children[1] : guard expr
            // children[2] : body stmt
            ASTNode *patternNode = caseNode->children[0].get();
            ASTNode *guardNode = caseNode->children[1].get();
            ASTNode *bodyNode = caseNode->children[2].get();
            chunk.emit_op(OP_DUP);

            if (patternNode != nullptr)
            {
                compile_expr(patternNode);
                chunk.emit_op(OP_CMP_EQ);
            }
            else
            {
                uint32_t cidx = chunk.add_const(RuntimeVal(BigDecimal("1")));
                chunk.emit_op(OP_PUSH_CONST);
                chunk.emit_u32(cidx);
            }
            compile_expr(guardNode);
            chunk.emit_op(OP_LOGIC_AND);
            size_t jmp_next_case = emit_jmp(OP_JMP_IF_FALSE);
            case_jmp_false_pos.push_back(jmp_next_case);
            chunk.emit_op(OP_POP);
            compile_stmt(bodyNode);
            size_t case_exit = emit_jmp(OP_JMP);
            patch_jmp(jmp_next_case);
            case_jmp_false_pos.push_back(case_exit);
        }
        chunk.emit_op(OP_POP);
        patch_jmp(switch_exit_jmp_pos);
        break;
    }
    case ASTNode::WHILE:
    {
        LoopPatch lp;
        lp.continue_pc = chunk.pc();
        lp.break_patch = 0;
        compile_expr(node->children[0].get());
        size_t jmp_false = emit_jmp(OP_JMP_IF_FALSE);
        loop_stack.push_back(lp);
        compile_stmt(node->children[1].get());
        size_t cont_jmp = emit_jmp(OP_JMP);
        patch_jmp(cont_jmp, lp.continue_pc);
        LoopPatch actual_lp = loop_stack.back();
        if (actual_lp.break_patch != 0)
        {
            patch_jmp(actual_lp.break_patch);
        }
        loop_stack.pop_back();
        patch_jmp(jmp_false);
        break;
    }
    case ASTNode::FOR_IN:
    {
        compile_expr(node->children[0].get());
        uint32_t varIdx = chunk.add_string(node->val);
        LoopPatch lp;
        lp.continue_pc = chunk.pc();
        lp.break_patch = 0;
        loop_stack.push_back(lp);
        chunk.emit_op(OP_FORIN_ITER);
        chunk.emit_u32(varIdx);
        size_t jmp_end = emit_jmp(OP_JMP_IF_FALSE);
        compile_stmt(node->children[1].get());
        size_t cont_jmp = emit_jmp(OP_JMP);
        patch_jmp(cont_jmp, lp.continue_pc);

        LoopPatch actual_lp = loop_stack.back();
        if (actual_lp.break_patch != 0)
        {
            patch_jmp(actual_lp.break_patch);
        }
        loop_stack.pop_back();
        patch_jmp(jmp_end);
        break;
    }
    case ASTNode::RETURN:
        compile_expr(node->children[0].get());
        chunk.emit_op(OP_RETURN);
        break;
    case ASTNode::BREAK:
    {
        auto &lp = loop_stack.back();
        size_t jp = emit_jmp(OP_JMP);
        lp.break_patch = jp;
        break;
    }
    case ASTNode::CONTINUE:
    {
        auto &lp = loop_stack.back();
        size_t jp = emit_jmp(OP_JMP);
        patch_jmp(jp, lp.continue_pc);
        break;
    }
    case ASTNode::LIT_NUM:
    case ASTNode::LIT_STR:
    case ASTNode::VAR:
    case ASTNode::BINARY:
    case ASTNode::UNARY:
    case ASTNode::ARRAY_LIT:
    case ASTNode::DICT_LIT:
    case ASTNode::INDEX:
    case ASTNode::MEMBER_ACCESS:
    case ASTNode::CALL:
    case ASTNode::MEMBER_CALL:
    case ASTNode::NEW_OBJ:
    case ASTNode::RANGE:
        compile_expr(node);
        chunk.emit_op(OP_POP);
        break;
    default:
        std::cerr << "[Compiler] unhandled stmt kind:" << (int)node->kind << "\n";
        break;
    }
}

void Compiler::compile_expr(ASTNode *node)
{
    if (!node)
        return;
    switch (node->kind)
    {
    case ASTNode::LIT_NUM:
    {
        uint32_t cidx = chunk.add_const(node->literal.clone());
        chunk.emit_op(OP_PUSH_CONST);
        chunk.emit_u32(cidx);
        break;
    }
    case ASTNode::LIT_STR:
    {
        uint32_t cidx = chunk.add_const(node->literal.clone());
        chunk.emit_op(OP_PUSH_CONST);
        chunk.emit_u32(cidx);
        break;
    }
    case ASTNode::VAR:
    {
        uint32_t sidx = chunk.add_string(node->val);
        chunk.emit_op(OP_LOAD_VAR);
        chunk.emit_u32(sidx);
        break;
    }
    case ASTNode::CALL:
    {
        for (size_t i = 0; i < node->children.size(); i++)
        {
            compile_expr(node->children[i].get());
        }
        uint32_t argCnt = (uint32_t)node->children.size();
        std::string fname = node->val;
        if (fname == "output" || fname == "outputLine" || fname == "input" || fname == "tonum" || fname == "tostring" || fname == "time" || fname == "time_ms" || fname == "sleep" || fname == "prminput" || fname == "args" || fname == "len" || fname == "conc" || fname == "eBuddha")
        {
            uint32_t cidx = chunk.add_const(RuntimeVal(fname));
            chunk.emit_op(OP_PUSH_CONST);
            chunk.emit_u32(cidx);
        }
        else
        {
            uint32_t nameIdx = chunk.add_string(fname);
            chunk.emit_op(OP_LOAD_VAR);
            chunk.emit_u32(nameIdx);
        }
        chunk.emit_op(OP_CALL);
        chunk.emit_u32(argCnt);
        break;
    }
    case ASTNode::MEMBER_CALL:
    {
        compile_expr(node->children[0].get());
        for (size_t i = 1; i < node->children.size(); i++)
        {
            compile_expr(node->children[i].get());
        }
        uint32_t argCnt = (uint32_t)(node->children.size() - 1);
        uint32_t memIdx = chunk.add_string(node->val);
        chunk.emit_op(OP_MEMBER_CALL);
        chunk.emit_u32(memIdx);
        chunk.emit_u32(argCnt);
        break;
    }
    case ASTNode::LAMBDA_EXPR:
    {
        VMClosure clos = compile_function(node);
        RuntimeVal closureObj = VM::wrap_closure(std::move(clos));
        uint32_t cidx = chunk.add_const(std::move(closureObj));
        chunk.emit_op(OP_PUSH_CONST);
        chunk.emit_u32(cidx);
        break;
    }
    case ASTNode::MEMBER_ACCESS:
    {
        compile_expr(node->children[0].get());
        uint32_t memIdx = chunk.add_string(node->val);
        chunk.emit_op(OP_LOAD_MEMBER);
        chunk.emit_u32(memIdx);
        break;
    }
    case ASTNode::INDEX:
    {
        compile_expr(node->children[0].get());
        uint32_t nameIdx = chunk.add_string(node->val);
        chunk.emit_op(OP_LOAD_VAR);
        chunk.emit_u32(nameIdx);
        chunk.emit_op(OP_LOAD_INDEX);
        break;
    }
    case ASTNode::NEW_OBJ:
    {
        for (size_t i = 0; i < node->children.size(); i++)
        {
            compile_expr(node->children[i].get());
        }
        uint32_t argCnt = (uint32_t)node->children.size();
        uint32_t clsIdx = chunk.add_string(node->val);
        chunk.emit_op(OP_LOAD_VAR);
        chunk.emit_u32(clsIdx);
        chunk.emit_op(OP_NEW_OBJ);
        chunk.emit_u32(argCnt);
        break;
    }
    case ASTNode::RANGE:
    {
        compile_expr(node->children[0].get());
        compile_expr(node->children[1].get());
        if (node->children.size() >= 3)
            compile_expr(node->children[2].get());
        else
        {
            chunk.emit_op(OP_PUSH_CONST);
            chunk.emit_u32(chunk.add_const(RuntimeVal(BigDecimal("1"))));
        }
        chunk.emit_op(OP_RANGE);
        break;
    }
    case ASTNode::ARRAY_LIT:
    {
        for (auto &c : node->children)
            compile_expr(c.get());
        chunk.emit_op(OP_ARRAY_LIT);
        chunk.emit_u32((uint32_t)node->children.size());
        break;
    }
    case ASTNode::DICT_LIT:
    {
        for (auto &c : node->children)
            compile_expr(c.get());
        chunk.emit_op(OP_DICT_LIT);
        chunk.emit_u32((uint32_t)(node->children.size() / 2));
        break;
    }
    case ASTNode::BINARY:
    {
        compile_expr(node->children[0].get());
        compile_expr(node->children[1].get());
        if (node->val == "+")
            chunk.emit_op(OP_ADD);
        else if (node->val == "-")
            chunk.emit_op(OP_SUB);
        else if (node->val == "*")
            chunk.emit_op(OP_MUL);
        else if (node->val == "/")
            chunk.emit_op(OP_DIV);
        else if (node->val == "==")
            chunk.emit_op(OP_CMP_EQ);
        else if (node->val == "!=")
            chunk.emit_op(OP_CMP_NEQ);
        else if (node->val == "<")
            chunk.emit_op(OP_CMP_LT);
        else if (node->val == ">")
            chunk.emit_op(OP_CMP_GT);
        else if (node->val == "<=")
            chunk.emit_op(OP_CMP_LE);
        else if (node->val == ">=")
            chunk.emit_op(OP_CMP_GE);
        else if (node->val == "&&" || node->val == "and")
            chunk.emit_op(OP_LOGIC_AND);
        else if (node->val == "||" || node->val == "or")
            chunk.emit_op(OP_LOGIC_OR);
        break;
    }
    case ASTNode::UNARY:
    {
        compile_expr(node->children[0].get());
        if (node->val == "!")
            chunk.emit_op(OP_LOGIC_NOT);
        else if (node->val == "-")
            chunk.emit_op(OP_NEG);
        break;
    }
    default:
        std::cerr << "[Compiler] unhandled expr kind:" << (int)node->kind << "\n";
        break;
    }
}

RuntimeVal VM::pop()
{
    auto v = std::move(stack.back());
    stack.pop_back();
    return v;
}

RuntimeVal &VM::peek(size_t off)
{
    return stack[stack.size() - 1 - off];
}

void VM::push(RuntimeVal v)
{
    stack.push_back(std::move(v));
}

uint8_t VM::read_u8()
{
    auto &fr = frame_stack.back();
    return fr.chunk->code[fr.pc++];
}

uint32_t VM::read_u32()
{
    uint32_t v = 0;
    v |= (static_cast<uint32_t>(read_u8()) << 0);
    v |= (static_cast<uint32_t>(read_u8()) << 8);
    v |= (static_cast<uint32_t>(read_u8()) << 16);
    v |= (static_cast<uint32_t>(read_u8()) << 24);
    return v;
}

int32_t VM::read_i32()
{
    return static_cast<int32_t>(read_u32());
}

RuntimeVal VM::run(ByteCodeChunk &bc, Scope *global_scope, Interpreter *interp)
{
    host_interp = interp;
    chunk = &bc;
    stack.clear();
    frame_stack.clear();

    VMFrame top_frame;
    top_frame.scope.parent = global_scope;
    top_frame.pc = 0;
    top_frame.chunk = &bc;
    frame_stack.push_back(std::move(top_frame));

    for (;;)
    {
        auto &fr = frame_stack.back();
        if (fr.pc >= fr.chunk->code.size())
            break;

        OpCode op = static_cast<OpCode>(read_u8());
        // std::cerr << "[DEBUG] pc=" << fr.pc << " stack_size=" << stack.size() << " op=" << (int)op << "\n";
        switch (op)
        {
        case OP_PUSH_CONST:
        {
            uint32_t idx = read_u32();
            push(fr.chunk->constant_pool[idx].clone());
            break;
        }
        case OP_POP:
            pop();
            break;
        case OP_LOAD_VAR:
        {
            uint32_t sid = read_u32();
            std::string name = fr.chunk->string_pool[sid];
            auto p = fr.scope.get(name);
            if (!p)
            {
                std::cerr << "[VM] undefined var:" << name << "\n";
                push(RuntimeVal());
            }
            else
            {
                push(p->clone());
            }
            break;
        }
        case OP_STORE_VAR:
        {
            uint32_t sid = read_u32();
            std::string name = fr.chunk->string_pool[sid];
            auto val = pop();
            fr.scope.set(name, std::move(val));
            break;
        }
        case OP_LOAD_MEMBER:
        {
            uint32_t mid = read_u32();
            std::string mname = fr.chunk->string_pool[mid];
            auto obj = pop();
            auto *o = obj.as_object();
            if (!o)
            {
                std::cerr << "[VM] load‑member need object\n";
                push(RuntimeVal());
                break;
            }
            auto it = o->value->members.find(mname);
            if (it == o->value->members.end())
            {
                std::cerr << "[VM] member not found " << mname << "\n";
                push(RuntimeVal());
                break;
            }
            push(it->second.clone());
            break;
        }
        case OP_STORE_MEMBER:
        {
            uint32_t mid = read_u32();
            std::string mname = fr.chunk->string_pool[mid];
            auto obj = pop();
            auto val = pop();
            auto *o = obj.as_object();
            if (!o)
            {
                std::cerr << "[VM] store‑member need object\n";
                break;
            }
            o->value->members[mname] = std::move(val);
            break;
        }
        case OP_LOAD_INDEX:
        {
            auto base = pop();
            auto idxv = pop();
            auto *arr = base.as_array();
            if (arr)
            {
                auto *num = idxv.as_num();
                size_t i;
                if (!safe_to_size_t(num->value, i) || i >= arr->value.size())
                {
                    std::cerr << "[VM] index out of bounds\n";
                    push(RuntimeVal());
                    break;
                }
                push(arr->value[i].clone());
            }
            else if (base.as_dict())
            {
                auto *d = base.as_dict();
                auto it = map_find_const(d->value, idxv);
                if (it == d->value.end())
                {
                    std::cerr << "[VM] dict key not found\n";
                    push(RuntimeVal());
                    break;
                }
                push(it->second.clone());
            }
            else
            {
                std::cerr << "[VM] index‑load need array/dict\n";
                push(RuntimeVal());
            }
            break;
        }
        case OP_STORE_INDEX:
        {
            auto base = pop();
            auto idxv = pop();
            auto val = pop();
            auto *arr = base.as_array();
            if (arr)
            {
                auto *num = idxv.as_num();
                size_t i;
                if (!safe_to_size_t(num->value, i) || i >= arr->value.size())
                {
                    std::cerr << "[VM] index out of bounds\n";
                    break;
                }
                arr->value[i] = std::move(val);
            }
            else if (base.as_dict())
            {
                auto *d = base.as_dict();
                d->value.insert_or_assign(std::move(idxv), std::move(val));
            }
            else
            {
                std::cerr << "[VM] index‑store need array/dict\n";
            }
            break;
        }
        case OP_DUP:
        {
            auto v = peek(0);
            push(v.clone());
            break;
        }
        case OP_ADD:
        {
            auto b = pop();
            auto a = pop();
            auto *na = a.as_num();
            auto *nb = b.as_num();
            if (na && nb)
            {
                push(RuntimeVal(na->value + nb->value));
            }
            else
            {
                auto *sa = a.as_str();
                auto *sb = b.as_str();
                if (sa && sb)
                {
                    push(RuntimeVal(sa->value + sb->value));
                }
                else
                {
                    std::cerr << "[VM] type mismatch for +\n";
                    push(RuntimeVal());
                }
            }
            break;
        }
        case OP_SUB:
        {
            auto b = pop();
            auto a = pop();
            push(RuntimeVal(a.as_num()->value - b.as_num()->value));
            break;
        }
        case OP_MUL:
        {
            auto b = pop();
            auto a = pop();
            push(RuntimeVal(a.as_num()->value * b.as_num()->value));
            break;
        }
        case OP_DIV:
        {
            auto b = pop();
            auto a = pop();
            push(RuntimeVal(a.as_num()->value / b.as_num()->value));
            break;
        }
        case OP_CMP_EQ:
        {
            auto b = pop();
            auto a = pop();
            push(RuntimeVal((a == b) ? BigDecimal("1") : BigDecimal("0")));
            break;
        }
        case OP_CMP_NEQ:
        {
            auto b = pop();
            auto a = pop();
            push(RuntimeVal((a != b) ? BigDecimal("1") : BigDecimal("0")));
            break;
        }
        case OP_CMP_LT:
        {
            auto b = pop();
            auto a = pop();
            push(RuntimeVal((a < b) ? BigDecimal("1") : BigDecimal("0")));
            break;
        }
        case OP_CMP_GT:
        {
            auto b = pop();
            auto a = pop();
            push(RuntimeVal((a > b) ? BigDecimal("1") : BigDecimal("0")));
            break;
        }
        case OP_CMP_LE:
        {
            auto b = pop();
            auto a = pop();
            push(RuntimeVal((a <= b) ? BigDecimal("1") : BigDecimal("0")));
            break;
        }
        case OP_CMP_GE:
        {
            auto b = pop();
            auto a = pop();
            push(RuntimeVal((a >= b) ? BigDecimal("1") : BigDecimal("0")));
            break;
        }
        case OP_LOGIC_AND:
        {
            auto b = pop();
            auto a = pop();
            auto *na = a.as_num();
            auto *nb = b.as_num();
            bool r = !(na->value == BigDecimal("0")) && !(nb->value == BigDecimal("0"));
            push(RuntimeVal(r ? BigDecimal("1") : BigDecimal("0")));
            break;
        }
        case OP_LOGIC_OR:
        {
            auto b = pop();
            auto a = pop();
            auto *na = a.as_num();
            auto *nb = b.as_num();
            bool r = !(na->value == BigDecimal("0")) || !(nb->value == BigDecimal("0"));
            push(RuntimeVal(r ? BigDecimal("1") : BigDecimal("0")));
            break;
        }
        case OP_LOGIC_NOT:
        {
            auto v = pop();
            auto *n = v.as_num();
            push(RuntimeVal((n->value == BigDecimal("0")) ? BigDecimal("1") : BigDecimal("0")));
            break;
        }
        case OP_NEG:
        {
            auto v = pop();
            BigDecimal x = v.as_num()->value;
            x.negative = !x.negative;
            push(RuntimeVal(x));
            break;
        }
        case OP_INC:
        {
            auto val = pop();
            auto *n = val.as_num();
            n->value = n->value + BigDecimal("1");
            push(std::move(val));
            break;
        }
        case OP_DEC:
        {
            auto val = pop();
            auto *n = val.as_num();
            n->value = n->value - BigDecimal("1");
            push(std::move(val));
            break;
        }
        case OP_COMPOUND_ADD:
        {
            auto lhs = pop();
            auto rhs = pop();
            auto *nl = lhs.as_num();
            auto *nr = rhs.as_num();
            nl->value = nl->value + nr->value;
            push(std::move(lhs));
            break;
        }
        case OP_COMPOUND_SUB:
        {
            auto lhs = pop();
            auto rhs = pop();
            auto *nl = lhs.as_num();
            auto *nr = rhs.as_num();
            nl->value = nl->value - nr->value;
            push(std::move(lhs));
            break;
        }
        case OP_COMPOUND_MUL:
        {
            auto lhs = pop();
            auto rhs = pop();
            auto *nl = lhs.as_num();
            auto *nr = rhs.as_num();
            nl->value = nl->value * nr->value;
            push(std::move(lhs));
            break;
        }
        case OP_COMPOUND_DIV:
        {
            auto lhs = pop();
            auto rhs = pop();
            auto *nl = lhs.as_num();
            auto *nr = rhs.as_num();
            nl->value = nl->value / nr->value;
            push(std::move(lhs));
            break;
        }
        case OP_JMP:
        {
            int32_t off = read_i32();
            fr.pc += off;
            break;
        }
        case OP_JMP_IF_FALSE:
        {
            int32_t off = read_i32();
            auto cond = pop();
            auto *cn = cond.as_num();
            if (cn->value == BigDecimal("0"))
            {
                fr.pc += off;
            }
            break;
        }
        case OP_JMP_IF_TRUE:
        {
            int32_t off = read_i32();
            auto cond = pop();
            auto *cn = cond.as_num();
            if (!(cn->value == BigDecimal("0")))
            {
                fr.pc += off;
            }
            break;
        }
        case OP_FORIN_ITER:
        {
            uint32_t varIdx = read_u32();
            std::string varName = fr.chunk->string_pool[varIdx];
            auto *arrp = fr.forin_array.as_array();
            if (arrp == nullptr)
            {
                fr.forin_array = pop();
                fr.forin_index = 0;
                arrp = fr.forin_array.as_array();
                if (!arrp)
                {
                    push(RuntimeVal(BigDecimal("0")));
                    break;
                }
            }
            if (fr.forin_index >= arrp->value.size())
            {
                push(RuntimeVal(BigDecimal("0")));
                fr.forin_array = RuntimeVal();
                fr.forin_index = 0;
            }
            else
            {
                fr.scope.set(varName, arrp->value[fr.forin_index].clone());
                fr.forin_index++;
                push(RuntimeVal(BigDecimal("1")));
            }
            break;
        }
        case OP_CLASS_META:
        {
            uint32_t nameIdx = read_u32();
            uint32_t superIdx = read_u32();
            uint32_t methodCount = read_u32();
            std::string className = fr.chunk->string_pool[nameIdx];
            std::string superName = fr.chunk->string_pool[superIdx];
            auto meta = std::make_shared<ClassMeta>();
            meta->name = className;
            meta->super_class_name = superName;
            meta->super_meta = host_interp->resolve_superclass(superName, &fr.scope, 0);
            for (uint32_t m = 0; m < methodCount; m++)
            {
                auto val = pop();
                uint64_t ptr;
                std::istringstream iss(val.as_str()->value);
                iss >> ptr;
                ASTNode *funcNode = reinterpret_cast<ASTNode *>(ptr);
                if (!funcNode || funcNode->kind != ASTNode::FUNC_DEF)
                    continue;
                std::string tag = funcNode->val;
                size_t sep = tag.find('|');
                std::string fname = tag.substr(0, sep);
                std::string mode = tag.substr(sep + 1);
                FuncT ft;
                size_t pcnt = funcNode->children.size() - 1;
                for (size_t i = 0; i < pcnt; i++)
                    ft.first.push_back(funcNode->children[i]->val);
                ft.second = funcNode->children.back().get();
                if (mode == "instance")
                    meta->instance_methods[fname] = ft;
                else if (mode == "static")
                    meta->static_methods[fname] = ft;
            }
            push(RuntimeVal(meta));
            break;
        }
        case OP_CALL:
        {
            uint32_t argCnt = read_u32();
            auto funcVal = pop();
            if (funcVal.type() == RtKind::STRING)
            {
                std::string fname = funcVal.as_str()->value;
                // output
                if (fname == "output")
                {
                    std::vector<RuntimeVal> tmp;
                    for (uint32_t i = 0; i < argCnt; i++)
                    {
                        tmp.push_back(pop());
                    }
                    std::reverse(tmp.begin(), tmp.end());
                    for (auto &a : tmp)
                    {
                        if (a.type() == RtKind::STRING)
                            std::cout << a.as_str()->value;
                        else
                            std::cout << a.to_string();
                    }
                    push(RuntimeVal());
                    break;
                }
                // outputLine
                if (fname == "outputLine")
                {
                    std::vector<RuntimeVal> tmp;
                    for (uint32_t i = 0; i < argCnt; i++)
                    {
                        tmp.push_back(pop());
                    }
                    std::reverse(tmp.begin(), tmp.end());
                    for (auto &a : tmp)
                    {
                        if (a.type() == RtKind::STRING)
                            std::cout << a.as_str()->value;
                        else
                            std::cout << a.to_string();
                    }
                    std::cout << "\n";
                    push(RuntimeVal());
                    break;
                }
                if (fname == "input")
                {
                    std::string s;
                    std::getline(std::cin, s);
                    push(RuntimeVal(s));
                    break;
                }
                if (fname == "tonum")
                {
                    auto arg = pop();
                    auto *s = arg.as_str();
                    BigDecimal num(s->value);
                    push(RuntimeVal(num));
                    break;
                }
                if (fname == "tostring")
                {
                    auto arg = pop();
                    push(RuntimeVal(arg.to_string()));
                    break;
                }
                if (fname == "time")
                {
                    auto now = std::chrono::system_clock::now();
                    auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                    push(RuntimeVal(BigDecimal(std::to_string(sec))));
                    break;
                }
                if (fname == "time_ms")
                {
                    auto now = std::chrono::system_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                    push(RuntimeVal(BigDecimal(std::to_string(ms))));
                    break;
                }
                if (fname == "sleep")
                {
                    auto arg = pop();
                    size_t ms;
                    safe_to_size_t(arg.as_num()->value, ms);
                    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                    push(RuntimeVal());
                    break;
                }
                if (fname == "args")
                {
                    if (argCnt != 0)
                    {
                        std::cerr << "[VM] args() takes no arguments\n";
                        push(RuntimeVal());
                        break;
                    }
                    Array arr;
                    for (auto &s : host_interp->cmd_args)
                    {
                        arr.push_back(RuntimeVal(s));
                    }
                    push(RuntimeVal(std::move(arr)));
                    break;
                }
                if (fname == "prminput")
                {
                    std::vector<RuntimeVal> tmp;
                    for (uint32_t i = 0; i < argCnt; i++)
                    {
                        tmp.push_back(pop());
                    }
                    std::reverse(tmp.begin(), tmp.end());
                    for (auto &a : tmp)
                    {
                        if (a.type() == RtKind::STRING)
                            std::cout << a.as_str()->value;
                        else
                            std::cout << a.to_string();
                    }
                    std::cout.flush();
                    std::string s;
                    std::getline(std::cin, s);
                    if (std::cin.fail())
                    {
                        std::cin.clear();
                        std::cerr << "Input error\n";
                        push(RuntimeVal(""));
                        break;
                    }
                    push(RuntimeVal(s));
                    break;
                }
                // len
                if (fname == "len")
                {
                    if (argCnt != 1)
                    {
                        std::cerr << "[VM] len() expects exactly one argument\n";
                        push(RuntimeVal());
                        break;
                    }
                    auto arg = pop();
                    if (auto *ap = arg.as_array())
                    {
                        push(RuntimeVal(BigDecimal(std::to_string(ap->value.size()))));
                    }
                    else if (auto *sp = arg.as_str())
                    {
                        push(RuntimeVal(BigDecimal(std::to_string(sp->value.size()))));
                    }
                    else
                    {
                        std::cerr << "[VM] len() expects array or string\n";
                        push(RuntimeVal());
                    }
                    break;
                }
                // conc
                if (fname == "conc")
                {
                    if (argCnt != 2)
                    {
                        std::cerr << "[VM] conc() expects exactly two arguments\n";
                        push(RuntimeVal());
                        break;
                    }
                    auto arg1 = pop();
                    auto arg0 = pop();
                    auto *arr0 = arg0.as_array();
                    if (!arr0)
                    {
                        std::cerr << "[VM] conc() first argument must be array\n";
                        push(RuntimeVal());
                        break;
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
                    push(RuntimeVal(std::move(res)));
                    break;
                }
                // buddha
                if (fname == "eBuddha")
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
                    std::cout << eB << std::endl;
                    push(RuntimeVal());
                    break;
                }
            }
            VMClosure *pClos = unwrap_closure(funcVal);
            if (pClos != nullptr)
            {
                std::vector<RuntimeVal> args;
                for (size_t i = 0; i < argCnt; i++)
                    args.push_back(pop());
                VMFrame newFrame;
                newFrame.scope.parent = &fr.scope;
                newFrame.chunk = &pClos->chunk;
                newFrame.pc = 0;
                int variadicIdx = pClos->variadicIndex;
                Array restArray;
                for (size_t i = 0; i < args.size(); i++)
                {
                    if (variadicIdx != -1 && (int)i >= variadicIdx)
                    {
                        restArray.push_back(std::move(args[i]));
                    }
                    else if ((int)i < (int)pClos->params.size())
                    {
                        newFrame.scope.set(pClos->params[i], std::move(args[i]));
                    }
                }
                if (variadicIdx != -1)
                {
                    std::string restParam = pClos->params[variadicIdx];
                    newFrame.scope.set(restParam, RuntimeVal(std::move(restArray)));
                }
                for (int i = (int)args.size(); i < variadicIdx; i++)
                {
                    newFrame.scope.set(pClos->params[i], RuntimeVal());
                }
                frame_stack.push_back(std::move(newFrame));
                break;
            }
            push(RuntimeVal());
            break;
        }
        case OP_MEMBER_CALL:
        {
            uint32_t memIdx = read_u32();
            uint32_t argCnt = read_u32();
            std::string mname = fr.chunk->string_pool[memIdx];
            auto base = pop();
            auto *obj = base.as_object();
            if (!obj)
            {
                std::cerr << "[VM] member‑call need object\n";
                push(RuntimeVal());
                break;
            }
            FuncT *ft = host_interp->lookup_instance_method(obj->value->meta.get(), mname);
            if (!ft)
            {
                std::cerr << "[VM] method " << mname << " not found\n";
                push(RuntimeVal());
                break;
            }
            VMFrame newFrame;
            newFrame.scope.parent = &fr.scope;
            newFrame.self = base.clone();
            newFrame.chunk = chunk;
            newFrame.scope.set("self", base.clone());
            for (int64_t i = (int64_t)argCnt - 1; i >= 0; i--)
            {
                auto a = pop();
                newFrame.scope.set(ft->first[i + 1], std::move(a));
            }
            frame_stack.push_back(std::move(newFrame));
            break;
        }
        case OP_RETURN:
        {
            RuntimeVal retv = pop();
            frame_stack.pop_back();
            if (frame_stack.empty())
            {
                return retv.clone();
            }
            push(retv.clone());
            break;
        }
        case OP_BREAK:
        case OP_CONTINUE:
            /* extra */
            std::cerr << "[VM] unreachable opcode break/continue\n";
            goto vm_exit;
        case OP_ARRAY_LIT:
        {
            uint32_t elemCnt = read_u32();
            Array arr;
            arr.reserve(elemCnt);
            for (uint32_t i = 0; i < elemCnt; i++)
            {
                arr.insert(arr.begin(), pop());
            }
            push(RuntimeVal(std::move(arr)));
            break;
        }
        case OP_DICT_LIT:
        {
            uint32_t pairCnt = read_u32();
            Dict d;
            for (uint32_t i = 0; i < pairCnt; i++)
            {
                auto v = pop();
                auto k = pop();
                d.insert_or_assign(std::move(k), std::move(v));
            }
            push(RuntimeVal(std::move(d)));
            break;
        }
        case OP_RANGE:
        {
            auto step = pop();
            auto end = pop();
            auto start = pop();
            Array arr;
            auto *s = start.as_num();
            auto *e = end.as_num();
            auto *st = step.as_num();
            BigDecimal cur = s->value;
            while (cur < e->value)
            {
                arr.push_back(RuntimeVal(cur));
                cur = cur + st->value;
            }
            push(RuntimeVal(std::move(arr)));
            break;
        }
        case OP_NEW_OBJ:
        {
            uint32_t argCnt = read_u32();
            auto clsVal = pop();
            auto *clsMeta = clsVal.as_classmeta();
            if (!clsMeta)
            {
                std::cerr << "[VM] new need class meta\n";
                push(RuntimeVal());
                break;
            }
            auto inst = std::make_shared<ObjectInstance>(clsMeta->value, std::unordered_map<std::string, RuntimeVal>{});
            RuntimeVal obj(inst);
            auto itInit = inst->meta->instance_methods.find("init");
            if (itInit != inst->meta->instance_methods.end())
            {
                VMFrame newFr;
                newFr.scope.parent = &fr.scope;
                newFr.self = obj.clone();
                newFr.chunk = chunk;
                newFr.scope.set("self", obj.clone());
                for (int64_t i = (int64_t)argCnt - 1; i >= 0; i--)
                {
                    auto arg = pop();
                    newFr.scope.set(itInit->second.first[i + 1], std::move(arg));
                }
                frame_stack.push_back(std::move(newFr));
            }
            push(std::move(obj));
            break;
        }
        case OP_IMPORT:
        {
            uint32_t strIdx = read_u32();
            std::string path = fr.chunk->string_pool[strIdx];
            host_interp->import_file(path);
            push(RuntimeVal());
            break;
        }
        case OP_HALT:
            goto vm_exit;
        default:
            std::cerr << "[VM] unknown opcode:" << (int)op << "\n";
            goto vm_exit;
        }
    }
vm_exit:
    if (stack.empty())
        return RuntimeVal();
    return stack.back().clone();
}

RuntimeVal Interpreter::eval(ASTNode *node, Scope *scope, EvalFrame & /*frame*/)
{
    Compiler comp;
    comp.compile(node);
    VM vm;
    return vm.run(comp.chunk, scope, this);
}