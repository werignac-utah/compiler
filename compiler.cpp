#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer/lexer.cpp"
#include "parser/parser.cpp"
#include "typechecker/typechecker.cpp"
#include "assembly/assembly.cpp"
#include "optimization/optimization.cpp"


bool find_flag(const char* flag_to_find, const unsigned int& flag_count, char**& flags)
{
    for(unsigned int i = 0; i < flag_count; i++)
        if (!strcmp(flags[i], flag_to_find))
            return true;
    return false;
}

unsigned char get_op_level(const unsigned int& flag_count, char**& flags)
{
    for(unsigned int i = 0; i < flag_count; i++)
    {
        char front[3];
        front[0] = flags[i][0];
        front[1] = flags[i][1];
        front[2] = '\0';
        if (!strcmp(front, "-O"))
            return flags[i][2] - '0';
    }
    return 0;
}


int main(int argc, char **argv) {
    if (argc < 2)
    {
        std::printf("William Erignac's JPL Compiler takes at least one argument:\n %s <filename>\n", argv[0]);
        return 0;
    }

    const char* filename = argv[1];

    unsigned int flag_count = argc - 2;
    char** flags = argv + 2;

    // Gotten from: https://stackoverflow.com/questions/18398167/how-to-copy-a-txt-file-to-a-char-array-in-c 
    // Get the source JPL code as a file.
    std::ifstream source_f(filename);
    // Get the source as a C++ string. 
    std::string source_s((std::istreambuf_iterator<char>(source_f)), 
        std::istreambuf_iterator<char>());
    // Get the source as a C string.
    const char* source_c = source_s.c_str();

    if (find_flag("-l", flag_count, flags))
    {
        Lexer::lexPrintAll(source_c);
        return 0;
    }
    

    if (find_flag("-p", flag_count, flags))
    {
        std::vector<Lexer::token>* v;
        try
        {
            v = Lexer::lexAll(source_c);
        }
        catch(const Lexer::LexerException& exception)
        {
            std::printf("Compilation failed\n");
            return 0;
        }

        std::cout << Parser::parseToString(v);

        destroy_token_list(v);
        return 0;
    }
    
    if (find_flag("-t", flag_count, flags))
    {
        std::vector<Lexer::token>* v;
        v = Lexer::lexAll(source_c);
        std::vector<std::unique_ptr<Parser::CmdNode>> tree;
        
        try
        {
            tree = Parser::parse(v);
        }
        catch(const Parser::ParserException& e)
        {
            std::printf("Compilation failed\n");
            destroy_token_list(v);
            return 0;
        }

        try
        {
            Typechecker::typecheck(tree);

            for(std::unique_ptr<Parser::CmdNode>& cmd : tree)
            {
                std::cout << cmd.get()->toString() + "\n";
            }
        }
        catch(const Typechecker::TypeException& e)
        {
            std::printf("Compilation failed\n");
            destroy_token_list(v);
            return 0;
        }

        std::printf("Compilation succeeded\n");
        destroy_token_list(v);
        return 0;
    }

    if (find_flag("-s", flag_count, flags))
    {
         std::vector<Lexer::token>* v;
        v = Lexer::lexAll(source_c);
        std::vector<std::unique_ptr<Parser::CmdNode>> tree;
        
        try
        {
            tree = Parser::parse(v);
        }
        catch(const Parser::ParserException& e)
        {
            std::printf("Compilation failed\n");
            destroy_token_list(v);
            return 0;
        }

        std::shared_ptr<Typechecker::Scope> scope;

        try
        {
            scope = Typechecker::typecheck(tree);
        }
        catch(const Typechecker::TypeException& e)
        {
            std::printf("Compilation failed\n");
            destroy_token_list(v);
            return 0;
        }

        destroy_token_list(v);

        if (get_op_level(flag_count, flags) > 1)
        {
            Optimization::ConstantPropagation cp;
            cp.visit_all_cmds(tree);
        }
        
        Compiler::Assembly assembly(*scope, get_op_level(flag_count, flags));
        std::shared_ptr<Compiler::AFunction> main_function = std::make_shared<Compiler::AFunction>(assembly);

        for (auto& command : tree)
            main_function->cg_cmd(command);

        assembly.add_function(main_function);

        std::cout << assembly.toString();

        std::printf("Compilation succeeded\n");
        
        return 0;
    }


    std::vector<Lexer::token>* v = Lexer::lexAll(source_c);

    std::vector<std::unique_ptr<Parser::CmdNode>> tree = Parser::parse(v);

    destroy_token_list(v);
    std::shared_ptr<Typechecker::Scope> scope = Typechecker::typecheck(tree);

    if (get_op_level(flag_count, flags) > 1)
    {
        Optimization::ConstantPropagation cp;
        cp.visit_all_cmds(tree);
    }

    Compiler::Assembly assembly(*scope, get_op_level(flag_count, flags));
    std::shared_ptr<Compiler::AFunction> main_function = std::make_shared<Compiler::AFunction>(assembly);

    for (auto& command : tree)
        main_function->cg_cmd(command);

    assembly.add_function(main_function);

    return 0;
}
