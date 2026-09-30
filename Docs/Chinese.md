# SandScript（Sand）语言参考文档
> 版本：**1.8.beta**｜解释器程序名：`Sandi`｜源码后缀：`.sand`
SandScript是一门C++实现的动态解释型脚本语言，支持大数高精度运算、数组、字典、函数、面向对象类与继承、预处理器宏、条件编译、模块导入。

## 目录
1. [快速开始](#1-快速开始)
2. [词法基础](#2-词法基础)
3. [数据类型](#3-数据类型)
4. [运算符与优先级](#4-运算符与优先级)
5. [语句语法](#5-语句语法)
6. [表达式](#6-表达式)
7. [函数](#7-函数)
8. [面向对象 Class](#8-面向对象-class)
9. [内置函数](#9-内置函数)
10. [预处理器指令](#10-预处理器指令)
11. [完整BNF语法](#11-完整bnf语法)
12. [常见示例代码](#12-常见示例代码)
13. [运行时注意事项](#13-运行时注意事项)

## 1 快速开始
### 运行命令行
```bash
# 执行脚本
Sandi test.sand

# 查看帮助语法
Sandi --help

# 版本
Sandi --version

# 版权
Sandi --copyright
Sandi --copyright-apache
```

### hello.sand
```sand
outputLine("Hello SandScript!");
```
运行：`Sandi hello.sand`

## 2 词法基础
### 注释
- `//` 行注释，从`//`到行末尾全部忽略
```sand
// 这是单行注释
output(123); // 行尾注释
```

### 标识符 IDENT
- 字母、下划线开头；后续可以是字母、数字、下划线、非ASCII字符（中文标识符可用）
- **区分大小写**

### 关键字
```
if else while for in func return break continue
output input range from import class static new super begin end
```

### 字符串字面量
双引号包裹`"..."`，支持转义：
- `\n` 换行
- `\\` 反斜杠
- `\"` 双引号
其他原样输出。
```sand
var s = "hello\nworld";
```

### 数字字面量
十进制，支持小数，高精度BigDecimal，无浮点数精度丢失。
```sand
123
-45.67
0.0001
```

### 块标记
代码块使用 `begin ... end`，**不是大括号`{}`**。

## 3 数据类型
|类型|说明|字面量示例|
|---|---|---|
|`NUMBER`|高精度十进制大数|`123`, `-0.5`|
|`STRING`|字符串|`"abc"`|
|`ARRAY`|动态数组 `[]`|`[1,2,"hi"]`|
|`DICT`|字典map，key支持数字/字符串，不支持数组字典做key|`{"a":1, 2:"b"}`|
|`FUNCTION`|用户函数|`func f() begin end`|
|`CLASS_META`|类元数据|`class A begin end`|
|`OBJECT`|类实例对象|`new A()`|
|`NIL`|空值，函数无返回默认nil| |

> 布尔：**没有单独bool类型；用数字`0`代表假，非0代表真**。

## 4 运算符与优先级（低→高）
|优先级|运算符|说明|
|---|---|---|
|1（最低）|`||`|逻辑或，短路求值|
|2|`&&`|逻辑与，短路求值|
|3|`== != < > <= >=`|比较|
|4|`+ -`|加减，`+`支持字符串拼接|
|5|`* /`|乘除，大数除法，除零抛运行时错误|
|6|`! -`|一元：逻辑非、负号|
|7（最高）|`()`调用 `[]`下标 `.`成员访问 `++ --`|后缀操作|

赋值运算符：`= += -= *= /=`，**赋值是语句不是表达式，不能嵌套**。

### 运算符列表
```
++  --  +=  -=  *=  /=
+  -  *  /
==  !=  <  >  <=  >=
&&  ||  !
=  .  ...
```

## 5 语句语法
> 所有普通语句末尾必须加分号 `;`，块内部语句也要分号；`begin/end`块不需要分号。

### 变量赋值
```sand
a = 10;
b = a + 20;
arr[0] = 99;          //下标赋值
obj.val = 123;        //对象成员赋值
```

### 复合赋值
```sand
x += 5;
x -= 2;
arr[1] *= 10;
```

### 自增自减
```sand
i++;
i--;
arr[2]++;
```

### if条件语句
```sand
if(cond)
begin
    output("yes");
end
else
begin
    output("no");
end
```
支持`else if`链式
```sand
if(a == 1)
begin end
else if(a == 2)
begin end
else
begin end
```

### while循环
```sand
while(i < 10)
begin
    output(i);
    i++;
end
```

### for‑in 遍历数组
> 只支持遍历数组；不支持字典直接遍历。
```sand
for x in [10,20,30]
begin
    output(x);
end
```

### break / continue
跳出循环、继续下一轮循环，只能在while/for‑in内部。
```sand
while(1)
begin
    break;
end
```

### return 返回
用于函数内部返回值。
```sand
func add(a,b)
begin
    return a + b;
end
```

### import导入模块
```sand
from "util.sand" import ;
```
导入另一个sand源码文件，执行全局代码，符号进入当前全局作用域。

## 6 表达式
### 字面量表达式
```sand
123                     //数字
"test"                  //字符串
[1, 2, "abc"]           //数组字面量
{"name":"tom", 1: true} //字典字面量
range(0,10)             //range生成数组 [0,1...9]
range(0,10,2)           //带步长 range(start,end,step)
```

### 下标访问数组/字典
```sand
arr[0]
dict["key"]
```

### 成员访问与成员调用
```sand
obj.name
obj.sayHi(1,2)
super.foo(); //子类内部调用父类同名方法
```

### 函数调用
```sand
add(1,2);
```

### 一元表达式
```sand
! 0
-123
```

## 7 函数
定义：`func 名字(参数列表) begin ... end`

### 普通函数
```sand
func sum(a, b)
begin
    return a + b;
end

output(sum(3,5));
```

### Lambda表达式
`lambda (pamams) begin ... end`

### 可变参数 `...rest`
`...`标记可变参数，剩余全部参数收集为数组给到rest变量。可变参数必须写在参数列表**最后一位**。
```sand
func total(a, b, ...rest)
begin
    return a + b;
end
```

> 参数数量不匹配会报运行时错误。函数内部拥有独立局部作用域；变量先找局部，再向外找父scope。

## 8 面向对象 Class
### 基础语法
```sand
class Person
begin
    func init(name)      //构造方法 new()时自动调用init
    begin
        self.name = name;
    end

    func sayHello()      //实例方法，第一个隐式参数self
    begin
        output("hi, ");
        output(self.name);
    end

    static func info()   //静态方法，无self
    begin
        output("Person static");
    end
end

//实例化
p = new Person("Alice");
p.sayHello();

//调用静态方法
Person.info();
```

- `self`：实例方法隐式第一个参数，代表当前对象实例，**不要手动写self参数**，parser自动插入self参数。
- `init`：构造函数；调用`new ClassName(args)`自动执行`init(args)`。
- `static func`：静态方法，不属于实例，通过类名调用，没有self。
- 对象使用`.`访问成员；对象成员可以任意读写。

### 类继承 `:`父类名
```sand
class Student : Person
begin
    func init(name, grade)
    begin
        super.init(name); //调用父类方法
        self.grade = grade;
    end

    func show()
    begin
        super.sayHello();
        output(self.grade);
    end
end

s = new Student("Bob", 6);
s.show();
```
> `super`只能在实例方法内部使用，用于调用父类的实例方法。

### #private伪私有（预处理器）
类内部写`#private a,b...;`，预处理器自动把标识符重命名为`__ClassName__a`，实现简单私有变量，外部无法直接访问。
```sand
class A
begin
#private secret;
    func init()
    begin
        self.secret = 123;
    end
end
```

## 9 内置函数
|函数|说明|
|---|---|
|`output(expr)`|输出内容，不自动换行|
|`outputLine(expr)`|输出并换行|
|`input()`|读取一行标准输入，返回字符串|
|`prminput(prompt)`|输出提示字符串，读取一行输入|
|`tonum(str)`|字符串转高精度数字；失败报错|
|`tostring(val)`|任意值转为字符串|
|`time()`|返回当前系统时间戳（秒，数字）|
|`time_ms()`|毫秒级时间戳|
|`sleep(ms)`|休眠指定毫秒数|
|`args()`|命令行参数|

> 目前没有内置数组push、length，需要自己封装函数实现。

## 10 预处理器指令（行首`#`开头）
预处理在词法解析之前执行，处理宏替换、include、条件编译、private重命名。

### `#from path include filename`
导入文件，递归预处理，防止循环重复导入。

### `#from ; include filename`
从exe所在目录导入；`;`代表使用exe目录作为基准。
```sand
#from ; include "lib.sand"
```

### `#define NAME text ... #enddef`
多行文本宏替换；`#enddef`结束宏定义。
```sand
#define PI 3.1415926
#enddef
output(PI);
```

### `#inline name=value`
内联常量。
```sand
#inline DEBUG=1
```

### 条件编译
```sand
#ifdef DEBUG
output("debug mode");
#else
output("release");
#endif

#ifndef RELEASE
//未定义RELEASE才编译
#endif
```
支持`#elif SYMBOL`。

### `#private var1,var2...;`
类内伪私有，预处理器改名。

> 预处理器只做文本替换，不做语法检查。

## 11 完整BNF语法
```ebnf
program        ::= { stmt } ;
stmt           ::= if_stmt
                 | while_stmt
                 | for_in_stmt
                 | func_def_stmt
                 | class_def_stmt
                 | import_stmt
                 | return_stmt
                 | break_stmt
                 | continue_stmt
                 | print_stmt
                 | input_stmt
                 | assign_stmt
                 | compound_assign_stmt
                 | incdec_stmt
                 | index_assign_stmt
                 | member_assign_stmt
                 | expr_stmt ;

if_stmt        ::= "if" "(" expr ")" block [ "else" (if_stmt | block) ] ;
while_stmt     ::= "while" "(" expr ")" block ;
for_in_stmt    ::= "for" IDENT "in" expr block ;
func_def_stmt  ::= "func" IDENT "(" param_list ")" block ;
param_list     ::= [ param ( "," param )* ] ;
param          ::= IDENT | "..." IDENT ;
class_def_stmt ::= "class" IDENT [ ":" IDENT ] "begin" { class_member } "end" ;
class_member   ::= [ "static" ] "func" IDENT "(" param_list ")" block ;
import_stmt    ::= "from" STR "import" ";" ;
return_stmt    ::= "return" expr ";" ;
break_stmt     ::= "break" ";" ;
continue_stmt  ::= "continue" ";" ;
print_stmt     ::= "output" "(" expr ")" ";" ;
input_stmt     ::= "input" "(" ")" ";" ;
assign_stmt        ::= IDENT "=" expr ";" ;
compound_assign_stmt ::= lvalue_compound op_compound expr ";" ;
incdec_stmt        ::= lvalue_incdec ( "++" | "--" ) ";" ;
index_assign_stmt  ::= IDENT "[" expr "]" "=" expr ";" ;
member_assign_stmt ::= primary "." IDENT "=" expr ";" ;
expr_stmt      ::= expr ";" ;
block          ::= "begin" { stmt } "end" ;

expr           ::= logic_or ;
logic_or       ::= logic_and { "||"|"or" logic_and } ;
logic_and      ::= compare { "&&"|"and" compare } ;
compare        ::= add { ( "==" | "!=" | "<" | ">" | "<=" | ">=" ) add } ;
add            ::= mul { ( "+" | "-" ) mul } ;
mul            ::= unary { ( "*" | "/" ) unary } ;
unary          ::= ( "-" | "!" |"not") unary | primary ;

primary        ::= NUM
                 | STR
                 | "(" expr ")"
                 | array_lit
                 | dict_lit
                 | range_call
                 | lambda_expr
                 | call_or_index ;
lambda_expr    ::= "lambda" "(" param_list ")" block ;
array_lit      ::= "[" [ expr ( "," expr )* ] "]" ;
dict_lit       ::= "{" [ expr ":" expr ( "," expr ":" expr )* ] "}" ;
range_call     ::= "range" "(" expr "," expr [ "," expr ] ")" ;
call_or_index  ::= atom { postfix } ;
atom           ::= IDENT | "new" IDENT "(" [ expr ( "," expr )* ] ")" | "super" ;
postfix        ::= "(" [ expr ( "," expr )* ] ")"
                 | "[" expr "]"
                 | "." IDENT [ "(" [ expr ( "," expr )* ] ")" ] ;
```

## 12 常见示例代码
### 示例1：阶乘函数
```sand
func fact(n)
begin
    res = 1;
    i = 1;
    while(i <= n)
    begin
        res = res * i;
        i++;
    end
    return res;
end

outputLine(fact(6));
```

### 示例2：类继承
```sand
class Animal
begin
    func init(name)
    begin
        self.name = name;
    end
    func speak()
    begin
        outputLine("animal:");
        outputLine(self.name);
    end
end

class Dog : Animal
begin
    func init(name)
    begin
        super.init(name);
    end
    func speak()
    begin
        super.speak();
        outputLine("wang wang");
    end
end

d = new Dog("wangcai");
d.speak();
```

### 示例3：数组字典
```sand
arr = [10, 20, 30];
arr[1] = 99;

d = {"a":100, "b":200};
outputLine(d["a"]);
```

## 13 运行时注意事项
1. **块必须使用`begin end`，不要使用`{}`**；大括号仅用于字典字面量。
2. 语句末尾分号`;`不可省略，表达式语句也需要分号。
3. 布尔没有true/false字面量，`0`假，非0数字为真。
4. 字典key**不能是数组/对象**，会报运行时错误。
5. `super`只能在实例方法内部，不能在静态方法、全局作用域使用。
6. 对象是引用语义；赋值对象变量不会拷贝对象，多个变量指向同一个实例。
7. 数组赋值：`a = arr`复制整个数组；对象赋值仅复制引用。
8. 除法0会抛出runtime异常。
9. 预处理器`#define`是纯文本替换，注意宏的空格与优先级。
10. `for‑in`遍历中修改循环变量会同步回原数组对应位置。