#include "Interpreter.h"
int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        std::cout << "SandScript interpreter 1.5.3.alpha\n";
        std::cout << "Usage: Sandi.exe script.sand\n";
        std::cout << "Example: Sandi.exe test.sand\n";
        return 1;
    }
    if (argv[1] == "--version")
    {
        std::cout << "1.5.3.alpha\n";
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
        std::cerr << "C++ STD EXCEPTION CAUGHT: " << e.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "UNKNOWN C++ EXCEPTION CAUGHT\n";
    }
    std::cout << std::flush;
    return 0;
}
