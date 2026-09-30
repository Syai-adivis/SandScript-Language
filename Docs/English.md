# SandScript (Sandi) Language Reference Documentation
> Version: **1.8.beta** | Interpreter executable: `Sandi` | Source file extension: `.sand`
SandScript is a dynamically‑typed interpreted scripting language implemented in C++. It supports arbitrary‑precision decimal arithmetic, arrays, dictionaries, functions, object‑oriented classes with inheritance, preprocessor macros, conditional compilation and module importing.

## Table of Contents
1. [Getting Started](#1-getting-started)
2. [Lexical Basics](#2-lexical-basics)
3. [Data Types](#3-data-types)
4. [Operators & Precedence](#4-operators--precedence)
5. [Statements](#5-statements)
6. [Expressions](#6-expressions)
7. [Functions](#7-functions)
8. [Object‑Oriented Classes](#8-object‑oriented-classes)
9. [Built‑in Functions](#9-built-in-functions)
10. [Preprocessor Directives](#10-preprocessor-directives)
11. [Full BNF Grammar](#11-full-bnf-grammar)
12. [Code Examples](#12-code-examples)
13. [Runtime Notes & Pitfalls](#13-runtime-notes--pitfalls)

## 1 Getting Started
### Command‑line usage
```bash
# Run a script
Sandi test.sand

# Show grammar help
Sandi --help

# Show interpreter version
Sandi --version

# Show license information
Sandi --copyright
Sandi --copyright-apache
```

### Minimal example `hello.sand`
```sand
outputLine("Hello SandScript!");
```
Run:
```bash
Sandi hello.sand
```

## 2 Lexical Basics
### Comments
- `//` line comment: everything from `//` to end‑of‑line is ignored.
```sand
// single‑line comment
output(123); // trailing comment
```

### Identifiers
- Start with a letter or underscore; subsequent characters can be letters, digits, underscores, non‑ASCII characters (supports Chinese identifiers).
- **Case‑sensitive**.

### Keywords
```
if else while for in func return break continue
output input range from import class static new super begin end
```

### String literals
Double‑quoted `"..."`. Supported escape sequences:
- `\n` newline
- `\\` backslash
- `\"` double quote
Unknown escape sequences are output literally.
```sand
s = "hello\nworld";
```

### Numeric literals
Decimal integer / fractional numbers backed by high‑precision `BigDecimal`. No floating‑point precision loss.
```sand
123
-45.67
0.0001
```

### Code blocks
Code blocks are wrapped with `begin ... end`. **Curly braces `{}` are only for dictionary literals**.

## 3 Data Types
| Type | Description | Literal Example |
|---|---|---|
| `NUMBER` | Arbitrary‑precision decimal | `123`, `-0.5` |
| `STRING` | UTF‑8 string | `"abc"` |
| `ARRAY` | Dynamic ordered array | `[1, 2, "hi"]` |
| `DICT` | Key‑value map | `{"a":1, 2:"b"}` |
| `FUNCTION` | User‑defined function | `func f() begin end` |
| `CLASS_META` | Class metadata | `class A begin end` |
| `OBJECT` | Class instance object | `new A()` |
| `NIL` | Null value; default return for void operations | |

> Boolean semantics: **there are no dedicated boolean types**. Number `0` means false; any non‑zero number means true.

## 4 Operators & Precedence (low → high)
| Precedence | Operators | Description |
|---|---|---|
| 1 (lowest) | `||` | Logical OR (short‑circuit) |
| 2 | `&&` | Logical AND (short‑circuit) |
| 3 | `== != < > <= >=` | Comparison |
| 4 | `+ -` | Addition / subtraction; `+` also concatenates strings |
| 5 | `* /` | Multiplication / division; division‑by‑zero throws runtime error |
| 6 | `! -` | Unary: logical NOT, arithmetic negation |
| 7 (highest) | `()` call `[]` index `.` member `++ --` | Postfix operations |

Assignment operators: `= += -= *= /=`. Assignments are **statements, not expressions** and cannot be nested inside expressions.

Full operator list:
```
++  --  +=  -=  *=  /=
+  -  *  /
==  !=  <  >  <=  >=
&&  ||  !
=  .  ...
```

## 5 Statements
> Every regular statement must end with a semicolon `;`. Semicolons are **not** required after `begin` / before `end`.

### Variable assignment
```sand
a = 10;
b = a + 20;
arr[0] = 99;          // index assignment
obj.val = 123;        // object member assignment
```

### Compound assignment
```sand
x += 5;
x -= 2;
arr[1] *= 10;
```

### Increment / decrement
```sand
i++;
i--;
arr[2]++;
```

### If conditional
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
Chained `else if` is supported:
```sand
if(a == 1)
begin end
else if(a == 2)
begin end
else
begin end
```

### While loop
```sand
while(i < 10)
begin
    output(i);
    i++;
end
```

### For‑in loop (array iteration only)
> Iterates over arrays only. Cannot iterate dictionaries directly.
```sand
for x in [10,20,30]
begin
    output(x);
end
```

### break / continue
`break` exits the innermost loop; `continue` skips to next iteration. Valid only inside `while` / `for‑in`.
```sand
while(1)
begin
    break;
end
```

### return
Returns a value from inside a function.
```sand
func add(a,b)
begin
    return a + b;
end
```

### Module import
```sand
from "util.sand" import ;
```
Executes global‑scope code from another `.sand` source file; imported symbols enter the global scope.

## 6 Expressions
### Literals
```sand
123                     // number
"test"                  // string
[1, 2, "abc"]           // array literal
{"name":"tom", 1: 1}    // dict literal
range(0,10)             // generates array [0,1 ... 9]
range(0,10,2)           // range(start, end, step)
```

### Index access for array / dict
```sand
arr[0]
dict["key"]
```

### Member access & method call
```sand
obj.name
obj.sayHi(1,2)
super.foo(); // call superclass method inside instance method
```

### Function call
```sand
add(1,2);
```

### Unary expressions
```sand
! 0
-123
```

## 7 Functions
Definition syntax: `func name(paramList) begin ... end`

### Ordinary function
```sand
func sum(a, b)
begin
    return a + b;
end

output(sum(3,5));
```

## Lambda expression
`lambda (pamams) begin ... end`

### Variadic parameters `...rest`
`...` marks variadic capture. Remaining arguments are collected into an array. Variadic parameter **must appear last** in parameter list.
```sand
func total(a, b, ...rest)
begin
    return a + b;
end
```

Argument count mismatch raises runtime error. Functions create a local scope; variables resolve locally first then propagate to parent scope.

## 8 Object‑Oriented Classes
### Basic class definition
```sand
class Person
begin
    func init(name)      // constructor: auto‑invoked on new()
    begin
        self.name = name;
    end

    func sayHello()      // instance method: implicit self argument injected by parser
    begin
        output("hi, ");
        output(self.name);
    end

    static func info()   // static method: no self
    begin
        output("Person static");
    end
end

// instantiate
p = new Person("Alice");
p.sayHello();

// invoke static method
Person.info();
```

- `self`: implicit first parameter inside instance methods, points to current object instance. Do **not** write `self` in your parameter list manually.
- `init`: constructor method; automatically called when `new ClassName(args)` executes.
- `static func`: static method belongs to the class, not instances; invoked via class name.
- Object members can be freely read‑written with `.` operator.

### Class inheritance `: SuperClassName`
```sand
class Student : Person
begin
    func init(name, grade)
    begin
        super.init(name); // invoke superclass instance method
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

> `super` can **only** be used inside instance methods; forbidden in static methods or global scope.

### Preprocessor pseudo‑private `#private`
Inside a class block use `#private var1,var2...;`. The preprocessor renames identifiers to `__ClassName__varName` to simulate private members which cannot be accessed externally.
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

## 9 Built‑in Functions
| Function | Description |
|---|---|
| `output(expr)` | Print value without appending newline |
| `outputLine(expr)` | Print value followed by newline |
| `input()` | Read one line from stdin, returns string |
| `prminput(prompt)` | Print prompt string then read one input line |
| `tonum(str)` | Convert string to high‑precision number; error on invalid format |
| `tostring(val)` | Convert any runtime value to string representation |
| `time()` | Return UNIX timestamp in seconds as number |
| `time_ms()` | Return timestamp in milliseconds |
| `sleep(ms)` | Suspend execution for given milliseconds |
| `args()` | CLI argumment (string array) |

> There are no built‑in array length / push functions now; implement them with user‑defined functions.

## 10 Preprocessor Directives (lines starting with `#`)
Preprocessing runs **before lexing / parsing**. It handles text‑level macro substitution, file inclusion, conditional compilation and private‑member renaming.

### `#include filename`
Include and preprocess another source file; guards against cyclic duplicate includes.
```sand
#include "util.sand"
```

### `#from ; include filename`
Resolve include relative to interpreter executable directory.
```sand
#from ; include "lib.sand"
```

### `#define NAME text ... #enddef`
Multi‑line text macro definition, terminated by `#enddef`.
```sand
#define PI 3.1415926
#enddef
output(PI);
```

### `#inline name=value`
Inline costnant.
```sand
#inline DEBUG=1
```

### Conditional compilation
```sand
#ifdef DEBUG
output("debug mode");
#else
output("release");
#endif

#ifndef RELEASE
// compiled only if RELEASE symbol is undefined
#endif
```
`#elif SYMBOL` is also supported.

### `#private var1,var2...;`
Pseudo‑private variable renaming inside class blocks.

> Preprocessor performs raw text substitution; it does **not** validate syntax.

## 11 Full BNF Grammar
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
unary          ::= ( "-" | "!" | "not") unary | primary ;

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

## 12 Code Examples
### Factorial function
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

### Class inheritance sample
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

### Array & dictionary usage
```sand
arr = [10, 20, 30];
arr[1] = 99;

d = {"a":100, "b":200};
outputLine(d["a"]);
```

## 13 Runtime Notes & Pitfalls
1. Code blocks must use `begin ... end`. Curly braces `{}` are only for dictionary literals.
2. Semicolon `;` terminates every statement including bare‑expression statements.
3. No `true` / `false` literals; use numeric `0` for false and non‑zero for true.
4. Dictionary keys **cannot be array or object values**, this triggers runtime error.
5. `super` is only valid inside instance methods; forbidden in static methods and global scope.
6. Objects use reference semantics: assigning object variables copies the reference, not the instance. Multiple variables may point to one same object.
7. Array assignment `a = arr` performs deep copy of array contents; object assignment copies only reference pointer.
8. Division‑by‑zero throws runtime exception.
9. `#define` performs raw text substitution; be careful with operator precedence and whitespace in macro bodies.
10. `for‑in` iterates arrays only. Modifying the loop variable inside the body writes‑back to original array elements.