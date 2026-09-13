# SandScript 1.15 Alpha
> Syai 2026/9/13
> 这是Syai的私人项目，获得该文档及解释器时未经允许请不要分发
---
## 目录
1. [概述](#概述)
2. [运行方式](#运行方式)
3. [词法基础](#词法基础)
4. [数据类型](#数据类型)
5. [变量与作用域](#变量与作用域)
6. [运算符](#运算符)
7. [语句](#语句)
8. [内置函数](#内置函数)
9. [函数定义](#函数定义)
10. [数组](#数组)
11. [字典](#字典)
12. [循环](#循环)
13. [模块导入](#模块导入)
14. [注释](#注释)
15. [语法示例](#语法示例)
16. [错误与异常](#错误与异常)

## 概述
- 数值系统：全部使用`BigDecimal`高精度十进制，`BD_DIV_PRECISION=50`，除法保留50位小数精度。
- 文件后缀：推荐 `.sand`
- 块语法：`begin ... end` 代替大括号。
- 语句结尾必须带分号 `;`。
- 真值规则：数字`0`代表假，一切非0数字代表真；条件只接受数字。字符串、数组不能直接做if/while条件。

## 运行方式
```bash
Sandi test.sand
```

## 词法基础
### 标识符
- 由字母、数字、下划线`_`组成，不能以数字开头；支持非ASCII字符。
```sand
a, var1, _foo, 变量
```

### 关键字
```sand
if else while for in func return break continue output input range from import begin end prminput outputln
```

### 字面量
1. **数字字面量**：支持整数、小数
```sand
123;
3.14159;
-0.0001;
```
2. **字符串字面量**：双引号包裹，支持转义
- `\"` 双引号
- `\\` 反斜杠
- `\n` 换行
```sand
"hello world";
"line1\nline2";
```

## 数据类型
Sand一共6种运行时类型：

| 类型 | 说明 |
|---|---|
| `number` | 高精度`BigDecimal`数字，所有数值运算都是任意精度 |
| `string` | UTF‑8字符串 |
| `array` | 数组，可变容器，可以存放任意类型元素 |
| `dict` | 字典（map），key只能是number/string/nil；不能用数组、字典、函数做key |
| `func` | 用户自定义函数 |
| `nil` | 空值，无任何数据 |

> 注意：`nil`不等于数字0。

## 变量与作用域
### 赋值语法
```sand
x = 100;
msg = "hello";
arr = [1,2,3];
```

### 作用域规则
1. 全局作用域：脚本顶层。
2. 块作用域：`begin end`块、`for‑in`循环体、函数内部。
3. Scope查找规则：
   - set赋值：如果父作用域已经存在同名变量，则修改父域变量，不会在当前域新建。
   - get读取：向上一直查到全局，找不到报未定义变量。

```sand
a = 1;
begin
    a = 10; // 修改全局a，不是局部
end
output(a); //输出10
```

## 运算符
### 算术运算符
|运算符|说明|
|---|---|
|`+`|加法 / 字符串拼接|
|`-`|减法 / 一元负号|
|`*`|乘法|
|`/`|除法（50位小数精度）|

> 只有两个字符串可以`+`拼接；其它类型`+`只数字。

### 比较运算符
返回数字`1`(真)或`0`(假)
```sand
==   !=   <   >   <=   >=
```
> 数组、字典比较：`==`会逐元素比较内容；函数之间永远不相等。

### 逻辑运算符
逻辑运算只接受数字类型。`0`为假，非`0`为真。
|运算符|说明|
|---|---|
|`&&`|逻辑与，短路|
|`||`|逻辑或，短路|
|`!`|逻辑非：输入非0返回0；输入0返回1|

示例
```sand
a = 1;
b = 0;
c = a && b; // 0
d = a || b; //1
e = !b;     //1
```

### 复合赋值运算符
```sand
+=   -=   *=   /=
```
支持普通变量，也支持数组下标复合赋值。
```sand
n = 10;
n += 5;

arr = [2,3];
arr[0] *= 10;
```

### 自增自减
`++` `--`，只支持数字变量、数组数字元素。
> 只支持后置，没有前置。
```sand
i = 0;
i++;
i--;

arr[1]++;
```

## 语句
> 所有语句末尾**必须写分号`;`**。块使用`begin ... end`，分号补全以后再加。

### if条件语句
```sand
if(条件) begin
    // true分支
end
else begin
    // false分支
end
```
条件表达式**必须是数字**。

示例
```sand
x = 10;
if(x > 5) begin
    output("big");
end
else begin
    output("small");
end
```

### while循环
```sand
while(条件) begin
    //循环体
end
```

### break / continue
- `break;`跳出最内层循环；
- `continue;`直接进入下一轮循环。

```sand
i = 0;
while(i < 10) begin
    i++;
    if(i ==5) break;
end
```

## 循环 for‑in
遍历数组。语法：`for 变量 in 数组 begin ... end`
```sand
for item in [10,20,30] begin
    output(item);
end
```
> for‑in循环体内修改循环变量，会同步写回原始数组对应位置。

### range函数生成数组
`range(start, end)`；`range(start, end, step)`
生成数组，左闭右开。
```sand
r = range(0,5);       // [0,1,2,3,4]
r2 = range(0,10,2);   // [0,2,4,6,8]
```

## 数组
数组字面量 `[expr, expr, ...]`
```sand
arr = [1, "abc", nil, [2,3]];
```

下标访问 `arr[index]`，下标从0开始。
```sand
arr = [100,200];
output(arr[0]);
arr[1] = 999;
arr[0] += 1;
arr[1]++;
```
> 下标必须是非负整数数字；越界会运行时错误。

## 字典
字典字面量 `{key:value, key:value}`
key只能是数字、字符串、nil。**不能用数组、字典、函数做key**。
```sand
dict = {"name":"sand", "version":1};
output(dict["name"]);
dict["author"] = "someone";
```

## 函数定义
语法：
```sand
func add(a,b) begin
    return a + b;
end
```

调用：
```sand
s = add(3,5);
output(s);
```

### 可变参数函数 `...name`
参数列表最后写`...rest`接收剩余参数，得到数组。
```sand
func sum(...rest)
begin
    total = 0;
    for v in rest
    begin
        total += v;
    end
    return total;
end

output(sum(1,2,3,4)); //10
```

> return语句：`return expr;`，函数立刻返回值；没有return返回nil。

## 内置函数
### output(expr)
输出。字符串直接输出原始文本；其它类型输出`to_string()`形式。
```sand
output("hello");
output([1,2,3]);
```

### outputln(expr)
带换行的输出。字符串直接输出原始文本；其它类型输出`to_string()`形式。
```sand
output("hello");
output([1,2,3]);
```

### input()
读取一行控制台输入，返回字符串。
>在函数内调用有BUG
```sand
s = input();
```

### prminput(string)
输出提示，读取一行控制台输入，返回字符串。
```sand
s = prminput(">>>");
```

### tonum(str)
字符串转为高精度数字。参数必须字符串；格式错误抛出运行时错误。
```sand
n = tonum("123.45");
```

### tostring(value)
把任意值转为字符串表示。
```sand
s = tostring([1,2]);
```

## 模块导入
```sand
from "lib.san" import;//部分导入以后再加
```

## 注释
`//`单行注释，到行结束。
```sand
//这是注释
x = 10; //行尾注释
```

## 完整示例代码
### 示例1：求1‑10总和
```sand
func calc_sum() begin
    s = 0;
    for i in range(1,11) begin
        s += i;
    end
    return s;
end

res = calc_sum();
output(res);
```

### 示例2：字典
```sand
person = {"name":"Zhang", "age":20};
output(person["name"]);
person["age"]+1;
output(person["age"]);
```

### 示例3：输入输出
```sand
output("请输入数字：");
num = tonum(input());
output(num + 100);
```

## 语法速查表
```sand
//变量
a = 10;

//if
if(a>0) begin output(a); end

//while
i=0;
while(i<5) begin i++; end

//for‑in
for x in range(0,3) begin output(x); end

//函数
func f(a,b) begin return a+b; end

//数组
arr = [1,2,3];
arr[0] = 99;

//字典
d = {"k":123};

//自增
i++;
arr[1]--;

//导入
from "lib.sand" import;
```

## 运行时错误
常见错误类型：
1. 未定义变量
2. 数组下标越界 / 下标不是数字
3. 字典使用非法key（数组/函数）
4. 条件不是数字（if/while只接受number）
5. 函数参数数量不匹配
6. 除零错误
7. `tonum()`传入非法数字字符串
8. import找不到文件
> 报错格式：`[行号] Runtime error: xxx`

## 已知特性
1. 除法精度最高50。
2. 没有三元运算符，`&&=`/`||=`。
3. for‑in遍历数组，循环变量修改会回写原数组。
4. 函数为第一类值，但函数不能做字典key，`==`比较函数永远`0`。
5. 字符串只有`+`拼接。