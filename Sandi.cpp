#include "Interpreter.h"
int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        std::cout << "SandScript interpreter 1.5.1.alpha\n";
        std::cout << "Usage: Sandi.exe script.sand\n";
        std::cout << "Example: Sandi.exe test.sand\n";
        return 1;
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
    interp.eval(ast.get(), &interp.global);
    std::cout << std::flush;
    return 0;
}
