#include "Interpreter.h"
#include "SetColor.h"
#include "prepross.h"

#include <cstdlib>
namespace fs = std::filesystem;

static int count_token(const std::string &buf, const std::string &tok)
{
    int cnt = 0;
    size_t pos = 0;
    while ((pos = buf.find(tok, pos)) != std::string::npos)
    {
        cnt++;
        pos += tok.size();
    }
    return cnt;
}

static void repl_main(const std::string &exePathRaw)
{
    Console::setColor(Console::Color::Green);
    std::cout << "SandScript ";
    Console::setColor(Console::Color::Yellow);
    std::cout << "Shell \n";
    Console::setColor(Console::Color::BrightGreen);
    std::cout << " enter exit/quit to exit" << std::endl;
    std::cout << " enter copyright to get license information" << std::endl;
    std::cout << " enter help to get help information" << std::endl;
    Console::setColor(Console::Color::Reset);
    Interpreter interp;
    fs::path exePath = fs::absolute(exePathRaw);

    std::string multi_buf;
    int block_depth = 0;
    std::string line;

    while (true)
    {
        if (block_depth > 0)
        {
            Console::setColor(Console::Color::Cyan);
            std::cout << "  ...> ";
            Console::setColor(Console::Color::Reset);
        }
        else
        {
            Console::setColor(Console::Color::Cyan);
            std::cout << ">>> ";
            Console::setColor(Console::Color::Reset);
        }
        if (!std::getline(std::cin, line))
        {
            std::cout << "\n";
            break;
        }

        size_t first = line.find_first_not_of(" \t");
        std::string trim;
        if (first == std::string::npos)
            trim = "";
        else
            trim = line.substr(first);

        if (trim == "exit" || trim == "quit")
        {
            return;
        }
        if (trim == "copyright")
        {
            std::system("Sandi --copyright");
            std::system("Sandi --copyright-apache");
            continue;
        }
        if (trim == "help")
        {
            std::system("Sandi --help");
            continue;
        }
        multi_buf.append(line);
        multi_buf.append("\n");

        block_depth += count_token(line, "begin");
        block_depth -= count_token(line, "end");

        if (block_depth > 0)
        {
            continue;
        }

        std::string source = multi_buf;
        multi_buf.clear();
        block_depth = 0;

        try
        {

            std::string processed = preprocess_source(source, exePath);
            auto ast = parse_source(processed);
            EvalFrame frame;
            RuntimeVal ret = interp.eval(ast.get(), &interp.global, frame);
            if (ret.type() != RtKind::NIL)
            {
                Console::setColor(Console::Color::Cyan);
                std::cout << "=> " << ret.to_string() << "\n";
                Console::setColor(Console::Color::Reset);
            }
        }
        catch (std::exception &e)
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[REPL Error] " << e.what() << "\n";
            Console::setColor(Console::Color::Reset);
        }
        catch (...)
        {
            Console::setColor(Console::Color::Red);
            std::cerr << "[Unknown REPL Error] " << "\n";
            Console::setColor(Console::Color::Reset);
        }
    }
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    std::system("chcp 65001 > nul");
#endif
    repl_main(argv[0]);
    return 0;
}