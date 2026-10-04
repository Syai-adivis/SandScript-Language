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
BigDecimal::BigDecimal(const std::string &s)
{
    if (s.empty())
        throw std::runtime_error("BigDecimal parse: empty string");
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
[[maybe_unused]] static std::string add_str(const std::string &a, const std::string &b)
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
[[maybe_unused]] static std::string sub_str(const std::string &a, const std::string &b)
{
    std::string res;
    int borrow = 0;
    int i = (int)a.size() - 1;
    int j = (int)b.size() - 1;
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
[[maybe_unused]] static std::string mul_str(const std::string &a, const std::string &b)
{
    std::vector<int> vec(a.size() + b.size(), 0);
    for (int i = (int)a.size() - 1; i >= 0; --i)
    {
        for (int j = (int)b.size() - 1; j >= 0; --j)
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
[[maybe_unused]] static int cmp_raw(const std::string &a, const std::string &b)
{
    if (a.size() != b.size())
        return a.size() > b.size() ? 1 : -1;
    return a.compare(b);
}
[[maybe_unused]] static void div_raw(std::string a, std::string b, std::string &quotient, std::string &rem)
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
        std::string intsum = add_str(integer, other.integer);
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
            std::string intsub = sub_str(integer, other.integer);
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
        throw std::runtime_error("division by zero");
    BigDecimal a = *this;
    BigDecimal b = other;
    bool out_neg = (a.negative != b.negative);
    a.negative = false;
    b.negative = false;
    std::string num = a.integer + a.fractional;
    std::string den = b.integer + b.fractional;
    int norm_shift = BD_DIV_PRECISION + (int)b.fractional.size();
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
    DICT,
    FUNCTION,
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
    explicit FuncValue(FuncT v);
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

struct RuntimeVal
{
    std::unique_ptr<RuntimeValueBase> ptr;
    RuntimeVal();
    explicit RuntimeVal(std::unique_ptr<RuntimeValueBase> basePtr);
    explicit RuntimeVal(BigDecimal n);
    explicit RuntimeVal(std::string s);
    explicit RuntimeVal(ArrayValue::Array a);
    explicit RuntimeVal(DictValue::Dict d);
    explicit RuntimeVal(FuncT f);
    explicit RuntimeVal(std::shared_ptr<ClassMeta> cm);
    explicit RuntimeVal(std::shared_ptr<ObjectInstance> oi);
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
    DictValue *as_dict();
    const DictValue *as_dict() const;
    FuncValue *as_func();
    const FuncValue *as_func() const;
    ClassMetaValue *as_classmeta();
    const ClassMetaValue *as_classmeta() const;
    ObjectValue *as_object();
    const ObjectValue *as_object() const;
};

using Array = ArrayValue::Array;
using Dict = DictValue::Dict;

// ==========================================
inline NumValue::NumValue(BigDecimal v)
    : RuntimeValueBase(RtKind::NUMBER), value(std::move(v)) {}
inline StrValue::StrValue(std::string v)
    : RuntimeValueBase(RtKind::STRING), value(std::move(v)) {}
inline ArrayValue::ArrayValue(Array v)
    : RuntimeValueBase(RtKind::ARRAY), value(std::move(v)) {}
inline DictValue::DictValue(Dict v)
    : RuntimeValueBase(RtKind::DICT), value(std::move(v)) {}
inline FuncValue::FuncValue(FuncT v)
    : RuntimeValueBase(RtKind::FUNCTION), value(std::move(v)) {}
inline NilValue::NilValue()
    : RuntimeValueBase(RtKind::NIL) {}
inline ClassMetaValue::ClassMetaValue(std::shared_ptr<ClassMeta> v)
    : RuntimeValueBase(RtKind::CLASS_META), value(std::move(v)) {}

inline ObjectValue::ObjectValue(std::shared_ptr<ObjectInstance> oi)
    : RuntimeValueBase(RtKind::OBJECT), value(std::move(oi)) {}

inline RuntimeVal::RuntimeVal(std::shared_ptr<ObjectInstance> oi)
    : ptr(std::make_unique<ObjectValue>(std::move(oi))) {}

inline std::unique_ptr<RuntimeValueBase> ObjectValue::clone() const
{
    return std::make_unique<ObjectValue>(value);
}

inline std::string ObjectValue::to_string() const
{
    return "<object " + value->meta->name + ">";
}

// RuntimeVal 构造
inline RuntimeVal::RuntimeVal()
    : ptr(std::make_unique<NilValue>()) {}
inline RuntimeVal::RuntimeVal(std::unique_ptr<RuntimeValueBase> basePtr)
    : ptr(std::move(basePtr)) {}
inline RuntimeVal::RuntimeVal(BigDecimal n)
    : ptr(std::make_unique<NumValue>(std::move(n))) {}
inline RuntimeVal::RuntimeVal(std::string s)
    : ptr(std::make_unique<StrValue>(std::move(s))) {}
inline RuntimeVal::RuntimeVal(Array a)
    : ptr(std::make_unique<ArrayValue>(std::move(a))) {}
inline RuntimeVal::RuntimeVal(Dict d)
    : ptr(std::make_unique<DictValue>(std::move(d))) {}
inline RuntimeVal::RuntimeVal(FuncT f)
    : ptr(std::make_unique<FuncValue>(std::move(f))) {}
inline RuntimeVal::RuntimeVal(std::shared_ptr<ClassMeta> cm)
    : ptr(std::make_unique<ClassMetaValue>(std::move(cm))) {}

inline std::string RuntimeVal::to_string() const
{
    return ptr ? ptr->to_string() : "<null>";
}
inline RuntimeVal RuntimeVal::clone() const
{
    if (!ptr)
        return RuntimeVal();
    return RuntimeVal(ptr->clone());
}

inline NumValue *RuntimeVal::as_num()
{
    return dynamic_cast<NumValue *>(ptr.get());
}
inline const NumValue *RuntimeVal::as_num() const
{
    return dynamic_cast<const NumValue *>(ptr.get());
}
inline StrValue *RuntimeVal::as_str()
{
    return dynamic_cast<StrValue *>(ptr.get());
}
inline const StrValue *RuntimeVal::as_str() const
{
    return dynamic_cast<const StrValue *>(ptr.get());
}
inline ArrayValue *RuntimeVal::as_array()
{
    return dynamic_cast<ArrayValue *>(ptr.get());
}
inline const ArrayValue *RuntimeVal::as_array() const
{
    return dynamic_cast<const ArrayValue *>(ptr.get());
}
inline DictValue *RuntimeVal::as_dict()
{
    return dynamic_cast<DictValue *>(ptr.get());
}
inline const DictValue *RuntimeVal::as_dict() const
{
    return dynamic_cast<const DictValue *>(ptr.get());
}
inline FuncValue *RuntimeVal::as_func()
{
    return dynamic_cast<FuncValue *>(ptr.get());
}
inline const FuncValue *RuntimeVal::as_func() const
{
    return dynamic_cast<const FuncValue *>(ptr.get());
}
inline ClassMetaValue *RuntimeVal::as_classmeta()
{
    return dynamic_cast<ClassMetaValue *>(ptr.get());
}
inline const ClassMetaValue *RuntimeVal::as_classmeta() const
{
    return dynamic_cast<const ClassMetaValue *>(ptr.get());
}
inline ObjectValue *RuntimeVal::as_object()
{
    return dynamic_cast<ObjectValue *>(ptr.get());
}
inline const ObjectValue *RuntimeVal::as_object() const
{
    return dynamic_cast<const ObjectValue *>(ptr.get());
}

inline std::string NumValue::to_string() const
{
    return value.to_string();
}
inline std::unique_ptr<RuntimeValueBase> NumValue::clone() const
{
    return std::make_unique<NumValue>(value);
}
inline std::string StrValue::to_string() const
{
    return "\"" + value + "\"";
}
inline std::unique_ptr<RuntimeValueBase> StrValue::clone() const
{
    return std::make_unique<StrValue>(value);
}
inline std::string ArrayValue::to_string() const
{
    std::string res = "[";
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (i > 0)
            res += ",";
        res += value[i].to_string();
    }
    res += "]";
    return res;
}
inline std::unique_ptr<RuntimeValueBase> ArrayValue::clone() const
{
    Array newarr;
    newarr.reserve(value.size());
    for (auto const &elem : value)
    {
        newarr.push_back(elem.clone());
    }
    return std::make_unique<ArrayValue>(std::move(newarr));
}
inline std::string DictValue::to_string() const
{
    std::string res = "{";
    bool first = true;
    for (auto const &p : value)
    {
        if (!first)
            res += ",";
        first = false;
        res += p.first.to_string() + ":" + p.second.to_string();
    }
    res += "}";
    return res;
}
inline std::unique_ptr<RuntimeValueBase> DictValue::clone() const
{
    Dict newdict;
    for (auto const &p : value)
    {
        newdict.insert_or_assign(p.first.clone(), p.second.clone());
    }
    return std::make_unique<DictValue>(std::move(newdict));
}
inline std::string FuncValue::to_string() const
{
    return "<function>";
}
inline std::unique_ptr<RuntimeValueBase> FuncValue::clone() const
{
    return std::make_unique<FuncValue>(value);
}
inline std::string NilValue::to_string() const
{
    return "<null>";
}
inline std::unique_ptr<RuntimeValueBase> NilValue::clone() const
{
    return std::make_unique<NilValue>();
}
inline std::string ClassMetaValue::to_string() const
{
    return "<class " + value->name + ">";
}
inline std::unique_ptr<RuntimeValueBase> ClassMetaValue::clone() const
{
    return std::make_unique<ClassMetaValue>(value);
}

inline bool operator==(const RuntimeVal &a, const RuntimeVal &b)
{
    if (a.type() != b.type())
        return false;
    auto kind = a.type();
    switch (kind)
    {
    case RtKind::NUMBER:
        return a.as_num()->value == b.as_num()->value;
    case RtKind::STRING:
        return a.as_str()->value == b.as_str()->value;
    case RtKind::ARRAY:
    {
        const auto &arrA = a.as_array()->value;
        const auto &arrB = b.as_array()->value;
        if (arrA.size() != arrB.size())
            return false;
        for (size_t i = 0; i < arrA.size(); i++)
            if (!(arrA[i] == arrB[i]))
                return false;
        return true;
    }
    case RtKind::DICT:
    {
        const auto &dA = a.as_dict()->value;
        const auto &dB = b.as_dict()->value;
        if (dA.size() != dB.size())
            return false;
        for (const auto &kv : dA)
        {
            auto it = dB.find(kv.first);
            if (it == dB.end())
                return false;
            if (!(kv.second == it->second))
                return false;
        }
        return true;
    }
    case RtKind::FUNCTION:
        return a.as_func()->value.second == b.as_func()->value.second;
    case RtKind::NIL:
        return true;
    case RtKind::CLASS_META:
        return a.as_classmeta()->value.get() == b.as_classmeta()->value.get();
    case RtKind::OBJECT:
    {
        const auto &objA = a.as_object()->value;
        const auto &objB = b.as_object()->value;
        return objA.get() == objB.get();
    }
    default:
        return false;
    }
}
inline bool operator!=(const RuntimeVal &a, const RuntimeVal &b)
{
    return !(a == b);
}
inline bool operator<(const RuntimeVal &a, const RuntimeVal &b)
{
    if (a.type() != b.type())
        return a.type() < b.type();
    auto kind = a.type();
    switch (kind)
    {
    case RtKind::NUMBER:
        return a.as_num()->value < b.as_num()->value;
    case RtKind::STRING:
        return a.as_str()->value < b.as_str()->value;
    case RtKind::ARRAY:
    {
        const auto &arrA = a.as_array()->value;
        const auto &arrB = b.as_array()->value;
        size_t minlen = std::min(arrA.size(), arrB.size());
        for (size_t i = 0; i < minlen; i++)
        {
            if (arrA[i] < arrB[i])
                return true;
            if (arrB[i] < arrA[i])
                return false;
        }
        return arrA.size() < arrB.size();
    }
    case RtKind::DICT:
    {
        const auto &dA = a.as_dict()->value;
        const auto &dB = b.as_dict()->value;
        auto ia = dA.begin(), ib = dB.begin();
        for (; ia != dA.end() && ib != dB.end(); ++ia, ++ib)
        {
            if (ia->first < ib->first)
                return true;
            if (ib->first < ia->first)
                return false;
            if (ia->second < ib->second)
                return true;
            if (ib->second < ia->second)
                return false;
        }
        return dA.size() < dB.size();
    }
    case RtKind::FUNCTION:
        return a.as_func()->value.second < b.as_func()->value.second;
    case RtKind::NIL:
        return false;
    case RtKind::CLASS_META:
        return a.as_classmeta()->value.get() < b.as_classmeta()->value.get();
    case RtKind::OBJECT:
    {
        const auto &objA = a.as_object()->value;
        const auto &objB = b.as_object()->value;
        return objA.get() < objB.get();
    }
    default:
        return false;
    }
}
inline bool operator>(const RuntimeVal &a, const RuntimeVal &b)
{
    return b < a;
}
inline bool operator<=(const RuntimeVal &a, const RuntimeVal &b)
{
    return !(b < a);
}
inline bool operator>=(const RuntimeVal &a, const RuntimeVal &b)
{
    return !(a < b);
}

#endif // RUNTIME_VALUE_H