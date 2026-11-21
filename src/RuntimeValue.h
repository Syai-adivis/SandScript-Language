#ifndef RUNTIME_VALUE_H
#define RUNTIME_VALUE_H
#include "includes.h"
int BD_DIV_PRECISION = 50;

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


class ASTNode;
struct FuncParamInfo
{
    std::string name;
    ASTNode *default_expr = nullptr;
};

using FuncT = std::pair<std::vector<FuncParamInfo>, ASTNode *>;

using SlotFunc = FuncT;

struct SignalSlotTable
{
    std::vector<SlotFunc> connections;
};

struct ClassMeta
{
    std::string name;
    std::string super_class_name;
    std::shared_ptr<ClassMeta> super_meta;
    std::unordered_map<std::string, FuncT> instance_methods;
    std::unordered_map<std::string, FuncT> static_methods;
    ~ClassMeta() = default;
};

struct ObjectInstance
{
    std::shared_ptr<ClassMeta> meta;
    std::unordered_map<std::string, struct RuntimeVal> members;
    SignalSlotTable signalSlots;
    ObjectInstance() = default;
    ObjectInstance(std::shared_ptr<ClassMeta> m, std::unordered_map<std::string, struct RuntimeVal> mem)
        : meta(std::move(m)), members(std::move(mem)) {}
};

enum class RtKind
{
    NUMBER,
    STRING,
    ARRAY,
    RANGE,
    DICT,
    FUNCTION,
    TASK,
    NIL,
    CLASS_META,
    OBJECT
};

struct RuntimeValueBase
{
    RtKind kind;
    explicit RuntimeValueBase(RtKind k) : kind(k) {}
    virtual ~RuntimeValueBase() = default;
    virtual std::string to_string() const = 0;
    virtual std::unique_ptr<RuntimeValueBase> clone() const = 0;
};

struct LazyRangeValue : RuntimeValueBase
{
    BigDecimal start;
    BigDecimal end;
    BigDecimal step;

    LazyRangeValue(BigDecimal first, BigDecimal last, BigDecimal increment)
        : RuntimeValueBase(RtKind::RANGE),
          start(std::move(first)), end(std::move(last)), step(std::move(increment))
    {
        if (step == BigDecimal("0"))
            throw std::runtime_error("range step cannot be zero");
    }

    BigDecimal size() const
    {
        const BigDecimal distance = step.negative ? start - end : end - start;
        if (distance <= BigDecimal("0"))
            return BigDecimal("0");

        BigDecimal magnitude = step;
        magnitude.negative = false;
        BigDecimal count = distance / magnitude;
        if (!count.fractional.empty())
        {
            count.fractional.clear();
            count.normalize();
            count = count + BigDecimal("1");
        }
        return count;
    }

    bool contains(const BigDecimal &value) const
    {
        return step.negative ? value > end : value < end;
    }

    std::string to_string() const override
    {
        return "<range " + start.to_string() + "..." + end.to_string() + ">";
    }

    std::unique_ptr<RuntimeValueBase> clone() const override
    {
        return std::make_unique<LazyRangeValue>(start, end, step);
    }
};

struct RuntimeVal;

struct NumValue : RuntimeValueBase
{
    BigDecimal value;
    explicit NumValue(BigDecimal v);
    std::string to_string() const override;
    std::unique_ptr<RuntimeValueBase> clone() const override;
};
struct StrValue : RuntimeValueBase
{
    std::string value;
    explicit StrValue(std::string v);
    std::string to_string() const override;
    std::unique_ptr<RuntimeValueBase> clone() const override;
};
struct ArrayValue : RuntimeValueBase
{
    using Array = std::vector<RuntimeVal>;
    Array value;
    explicit ArrayValue(Array v);
    std::string to_string() const override;
    std::unique_ptr<RuntimeValueBase> clone() const override;
};
struct DictValue : RuntimeValueBase
{
    using Dict = std::map<RuntimeVal, RuntimeVal>;
    Dict value;
    explicit DictValue(Dict v);
    std::string to_string() const override;
    std::unique_ptr<RuntimeValueBase> clone() const override;
};
struct FuncValue : RuntimeValueBase
{
    FuncT value;
    bool is_worker;
    explicit FuncValue(FuncT v, bool worker = false);
    std::string to_string() const override;
    std::unique_ptr<RuntimeValueBase> clone() const override;
};
struct NilValue : RuntimeValueBase
{
    NilValue();
    std::string to_string() const override;
    std::unique_ptr<RuntimeValueBase> clone() const override;
};
struct ClassMetaValue : RuntimeValueBase
{
    std::shared_ptr<ClassMeta> value;
    explicit ClassMetaValue(std::shared_ptr<ClassMeta> v);
    std::string to_string() const override;
    std::unique_ptr<RuntimeValueBase> clone() const override;
};
struct ObjectValue : RuntimeValueBase
{
    std::shared_ptr<ObjectInstance> value;
    explicit ObjectValue(std::shared_ptr<ObjectInstance> oi);
    std::string to_string() const override;
    std::unique_ptr<RuntimeValueBase> clone() const override;
};

struct WorkerTaskState;

struct RuntimeVal
{
    std::unique_ptr<RuntimeValueBase> ptr;
    RuntimeVal();
    explicit RuntimeVal(std::unique_ptr<RuntimeValueBase> basePtr);
    explicit RuntimeVal(BigDecimal n);
    explicit RuntimeVal(std::string s);
    explicit RuntimeVal(ArrayValue::Array a);
    explicit RuntimeVal(BigDecimal start, BigDecimal end, BigDecimal step);
    explicit RuntimeVal(DictValue::Dict d);
    explicit RuntimeVal(FuncT f);
    RuntimeVal(FuncT f, bool worker);
    explicit RuntimeVal(std::shared_ptr<ClassMeta> cm);
    explicit RuntimeVal(std::shared_ptr<ObjectInstance> oi);
    explicit RuntimeVal(std::shared_ptr<WorkerTaskState> task);
    RuntimeVal(const RuntimeVal &other)
        : ptr(other.ptr ? other.ptr->clone() : nullptr)
    {
    }
    RuntimeVal &operator=(RuntimeVal &&) noexcept = default;
    RuntimeVal &operator=(const RuntimeVal &) = delete;

    RtKind type() const { return ptr ? ptr->kind : RtKind::NIL; }
    std::string to_string() const;
    RuntimeVal clone() const;

    NumValue *as_num();
    const NumValue *as_num() const;
    StrValue *as_str();
    const StrValue *as_str() const;
    ArrayValue *as_array();
    const ArrayValue *as_array() const;
    LazyRangeValue *as_range();
    const LazyRangeValue *as_range() const;
    DictValue *as_dict();
    const DictValue *as_dict() const;
    FuncValue *as_func();
    const FuncValue *as_func() const;
    ClassMetaValue *as_classmeta();
    const ClassMetaValue *as_classmeta() const;
    ObjectValue *as_object();
    const ObjectValue *as_object() const;
    struct TaskValue *as_task();
    const struct TaskValue *as_task() const;
};

bool operator==(const RuntimeVal &a, const RuntimeVal &b);
bool operator!=(const RuntimeVal &a, const RuntimeVal &b);
bool operator<(const RuntimeVal &a, const RuntimeVal &b);
bool operator>(const RuntimeVal &a, const RuntimeVal &b);
bool operator<= (const RuntimeVal &a, const RuntimeVal &b);
bool operator>= (const RuntimeVal &a, const RuntimeVal &b);

struct WorkerTaskState
{
    std::atomic<bool> done{false};
    std::atomic<bool> cancel_requested{false};
    std::mutex mutex;
    std::mutex join_mutex;
    std::thread thread;
    RuntimeVal result;
    std::exception_ptr failure;
};

struct TaskValue : RuntimeValueBase
{
    std::shared_ptr<WorkerTaskState> value;
    explicit TaskValue(std::shared_ptr<WorkerTaskState> state)
        : RuntimeValueBase(RtKind::TASK), value(std::move(state)) {}
    std::string to_string() const override { return "<worker task>"; }
    std::unique_ptr<RuntimeValueBase> clone() const override
    {
        return std::make_unique<TaskValue>(value);
    }
};

using Array = ArrayValue::Array;
using Dict = DictValue::Dict;

#endif // RUNTIME_VALUE_H