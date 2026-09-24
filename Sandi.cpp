#include "Interpreter.h"
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "SandScript interpreter 1.5.3.alpha\n";
        std::cout << "Usage: Sandi.exe script.sand\n";
        std::cout << "Example: Sandi.exe test.sand\n";
        return 0;
    }
    if (std::string(argv[1]) == "--version")
    {
        std::cout << "1.5.3.alpha\n";
        return 0;
    }
    if (std::string(argv[1]) == "--copyright")
    {
        std::string copyright = R"XXX(
            Copyright (c) 2026 Syai
            Permission is hereby granted, free of charge, to any person obtaining a copy
            of this software and associated documentation files (the "Software"), to deal
            in the Software without restriction, including without limitation the rights
            to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
            copies of the Software, and to permit persons to whom the Software is
            furnished to do so, subject to the following conditions:

            The above copyright notice and this permission notice shall be included in all
            copies or substantial portions of the Software.

            THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
            IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
            FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
            AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
            LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
            OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
            SOFTWARE.
        )XXX";
        std::cout << copyright << "\n";
        return 0;
    }
    if (std::string(argv[1]) == "--help")
    {
        std::cout << "SandScript interpreter 1.5.3.alpha\n";
        std::cout << "Usage: Sandi.exe script.sand\n";
        std::cout << "Example: Sandi.exe test.sand\n";
        std::string help = R"XXX(
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

if_stmt        ::= "if" "(" expr ")" block [ "else" if_stmt|block ] ;
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

(* Expression, precedence: low -> high *)
expr           ::= logic_or ;
logic_or       ::= logic_and { "||" logic_and } ;
logic_and      ::= compare { "&&" compare } ;
compare        ::= add { ( "==" | "!=" | "<" | ">" | "<=" | ">=" ) add } ;
add            ::= mul { ( "+" | "-" ) mul } ;
mul            ::= unary { ( "*" | "/" ) unary } ;
unary          ::= ( "-" | "!" ) unary | primary ;

primary        ::= NUM
                 | STR
                 | "(" expr ")"
                 | array_lit
                 | dict_lit
                 | range_call
                 | call_or_index ;

array_lit      ::= "[" [ expr ( "," expr )* ] "]" ;
dict_lit       ::= "{" [ expr ":" expr ( "," expr ":" expr )* ] "}" ;
range_call     ::= "range" "(" expr "," expr [ "," expr ] ")" ;

call_or_index  ::= atom { postfix } ;
atom           ::= IDENT | "new" IDENT "(" [ expr ( "," expr )* ] ")" | "super" ;
postfix        ::= "(" [ expr ( "," expr )* ] ")"       (* call *)
                 | "[" expr "]"                        (* index *)
                 | "." IDENT [ "(" [ expr ( "," expr )* ] ")" ] ; (* member / member-call *)

(* Lexer token rules *)
IDENT          ::= ( letter | "_" ) { letter | digit | "_" | non_ascii } ;
NUM            ::= digit+ [ "." digit+ ] ;
STR            ::= '"' { str_char } '"' ;
str_char       ::= ( any except '"' ) | escape_seq ;
escape_seq     ::= "\\" ( "n" | "\\" | '"' | any ) ;

letter         ::= "a"..."z" | "A"..."Z" ;
digit          ::= "0"..."9" ;
non_ascii      ::= #x80 ... #xFFFF ;

(* Keywords: if,else,while,for,in,func,return,break,continue,output,input,range,from,import,class,static,new,super,begin,end *)
(* Operators: ++,--,+=,-=,*=,/=,+,-,*,/,==,!=,<,>,<=,>=,&&,||,!,=,.,... *)
(* Punctuation: ( ) [ ] { } , : ; *)
        )XXX";
        std::cout << help << "\n";
        return 0;
    }
    std::ifstream fin(argv[1]);
    if (!fin.is_open())
    {
        std::cerr << "Error: cannot open file " << argv[1] << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string src = buffer.str();
    fin.close();

    auto ast = parse_source(src);
    Interpreter interp;
    EvalFrame top_frame;
    try
    {
        interp.eval(ast.get(), &interp.global, top_frame);
    }
    catch (const std::exception &e)
    {
        std::cerr << "EXCEPTION CAUGHT: " << e.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "UNKNOWN EXCEPTION CAUGHT\n";
    }
    std::cout << std::flush;
    return 0;
}
