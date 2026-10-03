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
enum OpCode : uint8_t
{
    OP_NOP,
    OP_PUSH_CONST, // u32 idx
    OP_POP,
    OP_DUP,
    OP_SWAP,
    OP_LOAD_VAR,     // u32
    OP_STORE_VAR,    // u32
    OP_LOAD_MEMBER,  // u32
    OP_STORE_MEMBER, // u32
    OP_LOAD_INDEX,
    OP_STORE_INDEX,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_CMP_EQ,
    OP_CMP_NEQ,
    OP_CMP_LT,
    OP_CMP_GT,
    OP_CMP_LE,
    OP_CMP_GE,
    OP_LOGIC_AND,
    OP_LOGIC_OR,
    OP_LOGIC_NOT,
    OP_NEG,
    OP_JMP,          // int32 offset
    OP_JMP_IF_FALSE, // int32 offset
    OP_JMP_IF_TRUE,  // int32 offset
    OP_CALL,         // u32 argCount
    OP_MEMBER_CALL,  // u32 argCount
    OP_RETURN,
    OP_BREAK,
    OP_CONTINUE,
    OP_ARRAY_LIT, // u32 elemCount
    OP_DICT_LIT,  // u32 pairCount
    OP_NEW_OBJ,   // u32 argCount
    OP_RANGE,
    OP_INC,
    OP_DEC,
    OP_COMPOUND_ADD,
    OP_COMPOUND_SUB,
    OP_COMPOUND_MUL,
    OP_COMPOUND_DIV,
    OP_FORIN_ITER, // u32 varNameIdx
    OP_CLASS_META, // u32 nameIdx, u32 superIdx, u32 methodCount, [modeIdx,fnameIdx,closureIdx]*N
    OP_SWITCH,     // u32 caseCount
    OP_CASE_PATTERN,
    OP_IMPORT,
    OP_PUSH_LOOP,
    OP_POP_LOOP,
    OP_HALT
};
struct ByteCodeChunk
{
    std::vector<uint8_t> code;
    std::vector<RuntimeVal> constant_pool;
    std::vector<std::string> string_pool;
    void emit_op(OpCode op)
    {
        code.push_back(static_cast<uint8_t>(op));
    }
    void emit_u32(uint32_t v)
    {
        code.push_back((v >> 0) & 0xFF);
        code.push_back((v >> 8) & 0xFF);
        code.push_back((v >> 16) & 0xFF);
        code.push_back((v >> 24) & 0xFF);
    }
    void emit_i32(int32_t v)
    {
        emit_u32(static_cast<uint32_t>(v));
    }
    uint32_t add_const(const RuntimeVal &v)
    {
        constant_pool.push_back(v.clone());
        return static_cast<uint32_t>(constant_pool.size() - 1);
    }
    uint32_t add_string(const std::string &s)
    {
        for (size_t i = 0; i < string_pool.size(); i++)
        {
            if (string_pool[i] == s)
                return static_cast<uint32_t>(i);
        }
        string_pool.push_back(s);
        return static_cast<uint32_t>(string_pool.size() - 1);
    }
    void patch_i32(size_t pos, int32_t offset)
    {
        uint32_t v = static_cast<uint32_t>(offset);
        code[pos + 0] = (v >> 0) & 0xFF;
        code[pos + 1] = (v >> 8) & 0xFF;
        code[pos + 2] = (v >> 16) & 0xFF;
        code[pos + 3] = (v >> 24) & 0xFF;
    }
    size_t pc() const { return code.size(); }
};
struct VMClosure
{
    ByteCodeChunk chunk;
    std::vector<std::string> params;
    int variadicIndex = -1;
};
struct LoopPatch
{
    size_t continue_pc;
    size_t break_patch;
};
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
struct VMFrame
{
    Scope scope;
    size_t pc = 0;
    EvalFrame ctrl;
    std::shared_ptr<ClassMeta> super_meta;
    RuntimeVal self;
    ByteCodeChunk *chunk = nullptr;
    RuntimeVal forin_array;
    size_t forin_index = 0;
};
struct Compiler
{
    ByteCodeChunk chunk;
    std::vector<LoopPatch> loop_stack;
    void compile(ASTNode *node);
    void compile_stmt(ASTNode *node);
    void compile_expr(ASTNode *node);
    size_t emit_jmp(OpCode op);
    void patch_jmp(size_t patch_pos);
    void patch_jmp(size_t patch_pos, size_t target_pc);
    VMClosure compile_function(ASTNode *funcNode);
};
struct VM
{
    ByteCodeChunk *chunk = nullptr;
    std::vector<RuntimeVal> stack;
    std::vector<VMFrame> frame_stack;
    std::vector<LoopPatch> vm_loop_stack;
    Interpreter *host_interp = nullptr;
    static RuntimeVal wrap_closure(VMClosure clos);
    static VMClosure *unwrap_closure(RuntimeVal &v);
    RuntimeVal pop();
    RuntimeVal &peek(size_t off = 0);
    void push(RuntimeVal v);
    uint8_t read_u8();
    uint32_t read_u32();
    int32_t read_i32();
    RuntimeVal run(ByteCodeChunk &bc, Scope *global_scope, Interpreter *interp);
};
struct Interpreter
{
    Scope global;
    FuncT *lookup_instance_method_from(ClassMeta *start_meta, const std::string &name);
    FuncT *lookup_instance_method(ClassMeta *meta, const std::string &name);
    FuncT *lookup_static_method(ClassMeta *meta, const std::string &name);
    std::shared_ptr<ClassMeta> resolve_superclass(const std::string &super_name, Scope *scope, size_t line);
    RuntimeVal *get_lvalue(RuntimeVal &root, ASTNode *idx_node, Scope *scope, EvalFrame &expr_frame, size_t line);
    std::vector<std::string> cmd_args;
    void import_file(const std::string &path);
    RuntimeVal eval(ASTNode *node, Scope *scope, EvalFrame &frame);
};