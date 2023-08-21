#include "lexer.h"
#include <regex>
#include <map>
#include <cstring>
#include <algorithm>
#include <string>

namespace Lexer
{
#pragma region String to TokenType Mappings
    /////////////////////////////////////////////////////////////////
    //                  String-TokenType Mappings                  //
    /////////////////////////////////////////////////////////////////

    struct RegexTypeMapping
    {
        std::regex first;
        TokenType second;
    };

    // A mapping of identifying strings to keyword token types.
    const RegexTypeMapping KEYWORDS[22] = {
        {std::regex("^array(?![a-zA-Z0-9_])"), ARRAY},
        {std::regex("^assert(?![a-zA-Z0-9_])"), ASSERT},
        {std::regex("^bool(?![a-zA-Z0-9_])"), BOOL},
        {std::regex("^else(?![a-zA-Z0-9_])"), ELSE},
        {std::regex("^false(?![a-zA-Z0-9_])"), FALSE},
        {std::regex("^float(?![a-zA-Z0-9_])"), FLOAT},
        {std::regex("^fn(?![a-zA-Z0-9_])"), FN},
        {std::regex("^if(?![a-zA-Z0-9_])"), IF},
        {std::regex("^image(?![a-zA-Z0-9_])"), IMAGE},
        {std::regex("^int(?![a-zA-Z0-9_])"), INT},
        {std::regex("^let(?![a-zA-Z0-9_])"), LET},
        {std::regex("^print(?![a-zA-Z0-9_])"), PRINT},
        {std::regex("^read(?![a-zA-Z0-9_])"), READ},
        {std::regex("^return(?![a-zA-Z0-9_])"), RETURN},
        {std::regex("^show(?![a-zA-Z0-9_])"), SHOW},
        {std::regex("^sum(?![a-zA-Z0-9_])"), SUM},
        {std::regex("^then(?![a-zA-Z0-9_])"), THEN},
        {std::regex("^time(?![a-zA-Z0-9_])"), TIME},
        {std::regex("^to(?![a-zA-Z0-9_])"), TO},
        {std::regex("^true(?![a-zA-Z0-9_])"), TRUE},
        {std::regex("^type(?![a-zA-Z0-9_])"), TYPE},
        {std::regex("^write(?![a-zA-Z0-9_])"), WRITE}
    };

    // A list of identifying strings that can represent an operator.
    const std::regex OPERATORS[] = {
        std::regex("^=="),
        std::regex("^>="),
        std::regex("^<="),
        std::regex("^>"),
        std::regex("^<"),
        std::regex("^!="),
        std::regex("^\\+"),
        std::regex("^-"),
        std::regex("^\\*(?!\\/)"),
        std::regex("^\\/(?!\\*)"),
        std::regex("^%"),
        std::regex("^&&"),
        std::regex("^\\|\\|"),
        std::regex("^!")
    };

    // A mapping of identifying strings to token types of punctuation, parentheses, and equals.
    const RegexTypeMapping OTHER[9] = {
        {std::regex("^:"), COLON},
        {std::regex("^,"), COMMA},
        {std::regex("^="), EQUALS},
        {std::regex("^\\("), LPAREN},
        {std::regex("^\\["), LSQUARE},
        {std::regex("^\\{"), LCURLY},
        {std::regex("^\\)"), RPAREN},
        {std::regex("^\\]"), RSQUARE},
        {std::regex("^\\}"), RCURLY}
    };

#pragma endregion

#pragma region Lexer Exception Implementation
    //////////////////////////////////////////////////////////////////////
    //                  Lexer Exception Implementation                  //
    //////////////////////////////////////////////////////////////////////

    LexerException::LexerException(const std::string& m, unsigned long line, unsigned long pos, const char* source)
    {
        std::cmatch cm;
        static std::regex rex("^[^ \n]*(?=[ \n]|$)");
        std::regex_search(source, cm, rex);

        std::string c_token(cm.str());
        std::string header = "\nEncountered Error at Lexing Step. Line " + std::to_string(line) + ", Position " + std::to_string(pos) + ", Token \"" + c_token + "\".\n";
        std::string middle = m;

        message = header + middle;
    }

    const char* LexerException::what () const noexcept
    {
        return message.c_str();
    }

#pragma endregion

    void clean_token(token& t)
    {
        free(t.text);
    }

#pragma region Lexer Functions
    ///////////////////////////////////////////////////////
    //                  Lexer Functions                  //
    ///////////////////////////////////////////////////////
    
    token lexRegex(LEX_FUNC_ARGS_NAMED, std::regex rex, TokenType token_type, std::string error_message)
    {
        std::cmatch cm;
        bool is_match = std::regex_search(source, cm, rex);

        if (! is_match)
            throw LexerException(error_message, line, pos, source);
        
        char* contents_copy = (char*) malloc((cm.length() + 1) * sizeof(char));
        strcpy(contents_copy, cm.str().c_str());

        source += cm.length();
        pos += cm.length();

        return {token_type, contents_copy, line, pos};
    }

#pragma region Keyword Lexer Functions
    // Tries to lex the next token as all JPL keywords.
    token const lexAllKeywords(LEX_FUNC_ARGS_NAMED)
    {
        token ret;
        bool returned = false;

        for (auto const& kvp : KEYWORDS)
        {
            try
            {
                ret = lexRegex(source, line, pos, kvp.first, kvp.second, "Could not lex token as keyword " + tokenTypeToString(kvp.second) + ".");
                returned = true;
                break;
            }
            catch(const LexerException& e){}
        }

        if (! returned)
        {
            std::string message = "Could not lex token as any keyword.";
            throw LexerException(message, line, pos, source);
        }

        return ret;
    }

#pragma endregion

#pragma region Symbol Lexer Functions
    // Tries to lex the next token as all JPL symbols.
    token const lexAllSymbols(LEX_FUNC_ARGS_NAMED)
    {
        token ret;
        bool returned = false;

        for (std::regex op_string : OPERATORS)
        {
            try
            {
                ret = lexRegex(source, line, pos, op_string, OP, "Could not lex token as operator.");
                returned = true;
                break;
            }
            catch(const LexerException& e){}
        }

        if (! returned)
        {
            for (auto const& kvp : OTHER)
            {
                try
                {
                    ret = lexRegex(source, line, pos, kvp.first, kvp.second, "Could not lex token as keyword " + tokenTypeToString(kvp.second) + ".");
                    returned = true;
                    break;
                }
                catch(const LexerException& e){}
            }
        }

        if (! returned)
        {
            std::string message = "Could not lex token as any symbol.";
            throw LexerException(message, line, pos, source);
        }

        return ret;
    }

#pragma endregion
    char* getNextSegment(char* start, char* end)
    {
        while (start != end)
        {
            if (*start == ' ' || *start == '\n')
                return start;
            start++;
        }
        return end;
    }
    
    // Tries to lex the next token as a JPL string.
    token const lexString(LEX_FUNC_ARGS_NAMED)
    {
        static std::regex rex = std::regex("^\"[^\\n\"]*\"");
        return lexRegex(source, line, pos, rex, STRING, "Could not lex token as string value.");
    }
    // Tries to lex the next token as a JPL float.
    token const lexFloatVal(LEX_FUNC_ARGS_NAMED)
    {
        std::cmatch cm;
        static std::regex rex(R"(^[0-9]+\.[0-9]*|^\.[0-9]+)");
        return lexRegex(source, line, pos, rex, FLOATVAL, "Could not lex token as float value.");
    }
    // Tries to lex the next token as a JPL int.
    token const lexIntVal(LEX_FUNC_ARGS_NAMED)
    {
        static std::regex rex("^[\\d]+(?![\\.\\d])");
        return lexRegex(source, line, pos, rex, INTVAL, "Could not lex token as int value");
    }
    // Tries to lex the next token as a JPL variable.
    token const lexVariable(LEX_FUNC_ARGS_NAMED)
    {
        static std::regex rex = std::regex("^[a-zA-Z_][a-zA-Z0-9_\\.]*");
        return lexRegex(source, line, pos, rex, VARIABLE, "Could not lex token as variable.");
    }
    // Tries to lex whitespace.
    token const lexWhitespace(LEX_FUNC_ARGS_NAMED)
    {
        static std::string newline = "\n";
        
        static std::string contline = "\\\\\n";
        static std::string space = " ";
        static std::string one_line_comment = R"(\/\/.*\n)";
        static std::string block_comments = R"(\/\*([^\*]|\*[^\/])*\*\/)";

        static std::regex white_space("^((" + newline + ")|(" + contline + ")|(" + space + ")|(" + one_line_comment + ")|(" + block_comments + "))*");
        // Check for a newline at the start of the string, a newline preceeded by a non-backslash, a newline after a one-line comment,
        // or a newline in block comment. 
        static std::regex newline_check("^\n|[^\\\\]\n|//.*\n|/\\*([^\\*]|\\*[^/])*\n([^\\*]|\\*[^/])*\\*/");

        std::cmatch cm;
        std::regex_search(source, cm, white_space);

        std::cmatch _;

        bool contains_newline = (cm.length() != 0) && (std::regex_search(cm.str().c_str(), _, newline_check));
        
        token t;

        if (contains_newline)
        {

            t = {NEWLINE, strdup("\n"), line, pos};

            long new_pos = 0;
            
            for (char c : cm.str())
            {
                if (c == '\n')
                {
                    line++;
                    new_pos = 0;
                }
                else
                    new_pos++;
            }

            pos = new_pos;
        }
        else
        {
            t = {NONE, strdup(""), line, pos};
            pos += cm.length();
        }
        
        source += cm.length();

        return t;
    }

    const token (*SUB_LEX_FUNCTIONS[])(LEX_FUNC_ARGS) = {
        lexAllKeywords,
        lexAllSymbols,
        lexString,
        lexFloatVal,
        lexIntVal,
        lexVariable
    };

    token lexToken(LEX_FUNC_ARGS_NAMED)
    {
        bool lexed = false;
        token ret;

        for (auto lex_func : SUB_LEX_FUNCTIONS)
        {
            try
            {
                ret = lex_func(source, line, pos);
                lexed = true;
                break;
            }
            catch(const LexerException& e) { }
        }

        if (! lexed)
        {
            std::string message = "Could not recognize token.";
            throw LexerException(message, line, pos, source);
        }

        return ret;
    }

    // Check that all tokens are valid.
    void lexPreprocess(const char*& source)
    {
        std::cmatch cm;
        static std::regex rex("[\n -~]");
        unsigned long line = 0;
        unsigned long pos = 0;
        for (int i = 0; i < std::strlen(source); i++)
        {
            char character[2] = { source[i], '\0'};
            if (!std::regex_match(character, cm, rex))
            {
                throw LexerException("Not all characters supported.", line, pos, source + i);
            }
            else if(source[i] == '\n')
            {
                line++;
                pos = 0;
            }
            else
                pos++;
        }
    }

    std::vector<token>* lexAll(const char* source)
    {
        lexPreprocess(source);   

        size_t source_len = strlen(source);
        char* source_copy = (char*) malloc(sizeof(char) * (source_len + 1));
        strcpy(source_copy, source);
        char* source_pointer = source_copy;

        unsigned long line = 0;
        unsigned long pos = 0;

        std::vector<token>* token_list = new std::vector<token>();

        token ret = lexWhitespace(source_pointer, line, pos);
        if (ret.type != NONE)
            token_list->push_back(ret);
        else
            clean_token(ret);

        char* end = source_copy + source_len;

        while (source_pointer != end)
        {
            char* next_segment_end = getNextSegment(source_pointer, end);
            int segment_size = next_segment_end - source_pointer;
            char* segment = (char*) malloc(sizeof(char) * (segment_size + 1));
            char* segmover = segment;
            strncpy(segment, source_pointer, segment_size);
            segment[segment_size] = '\0';
            source_pointer += segment_size;

            while(segmover != segment + segment_size)
            {
                try
                {
                    ret = lexToken(segmover, line, pos);
                }
                catch(const LexerException& e)
                {
                    free(segment);
                    throw;
                }
                
                token_list->push_back(ret);
            }

            free(segment);

            ret = lexWhitespace(source_pointer, line, pos);
            if (ret.type != NONE)
                token_list->push_back(ret);
            else
                clean_token(ret);
        }

        token_list->push_back({END_OF_FILE, strdup(""), 0, 0});
        free(source_copy);
        return token_list;
    }

    std::string tokenTypeToString(TokenType to_convert)
    {
        switch(to_convert)
        {
            case ARRAY:
                return "ARRAY";
            case ASSERT:
                return "ASSERT";
            case BOOL:
                return "BOOL";
            case ELSE:
                return "ELSE";
            case FALSE:
                return "FALSE";
            case FLOAT:
                return "FLOAT";
            case FN:
                return "FN";
            case IF:
                return "IF";
            case IMAGE:
                return "IMAGE";
            case INT:
                return "INT";
            case LET:
                return "LET";
            case PRINT:
                return "PRINT";
            case READ:
                return "READ";
            case RETURN:
                return "RETURN";
            case SHOW:
                return "SHOW";
            case SUM:
                return "SUM";
            case THEN:
                return "THEN";
            case TIME:
                return "TIME";
            case TO:
                return "TO";
            case TRUE:
                return "TRUE";
            case TYPE:
                return "TYPE";
            case WRITE:
                return "WRITE";
            case COLON:
                return "COLON";
            case LCURLY:
                return "LCURLY";
            case RCURLY:
                return "RCURLY";
            case LPAREN:
                return "LPAREN";
            case RPAREN:
                return "RPAREN";
            case COMMA:
                return "COMMA";
            case LSQUARE:
                return "LSQUARE";
            case RSQUARE:
                return "RSQUARE";
            case EQUALS:
                return "EQUALS";
            case STRING:
                return "STRING";
            case INTVAL:
                return "INTVAL";
            case FLOATVAL:
                return "FLOATVAL";
            case VARIABLE:
                return "VARIABLE";
            case OP:
                return "OP";
            case NEWLINE:
                return "NEWLINE";
            case END_OF_FILE:
                return "END_OF_FILE";
            default:
                return "";
        }
    }

    void print_token(token t)
    {
        std::string enum_name = tokenTypeToString(t.type);

        switch(t.type)
        {
            case NEWLINE:
            case END_OF_FILE:
                std::printf("%s\n", enum_name.c_str());
                break;
            default:
                std::printf("%s '%s'\n", enum_name.c_str(), t.text);
                break;
        }
    }

    void lexPrintAll(const char* source)
    {
        try
        {
            lexPreprocess(source);
        }
        catch(const LexerException& e)
        {
            std::printf("Compilation failed\n");
            return;
        }

        size_t source_len = strlen(source);
        char* source_copy = (char*) malloc(sizeof(char) * (source_len + 1));
        strcpy(source_copy, source);
        char* source_pointer = source_copy;

        unsigned long line = 0;
        unsigned long pos = 0;

        token ret;
        
        try
        {
            ret = lexWhitespace(source_pointer, line, pos);
        }
        catch(const LexerException& e)
        {
            free(source_copy);
            std::printf("Compilation failed\n");
            return;
        }

        if (ret.type != NONE)
            print_token(ret);
        clean_token(ret);

        while (source_pointer != source_copy + source_len)
        {
            try
            {
                ret = lexToken(source_pointer, line, pos);
            }
            catch(const LexerException& e)
            {
                free(source_copy);
                std::printf("Compilation failed\n");
                return;
            }
            
            print_token(ret);
            clean_token(ret);
            try
            {
                ret = lexWhitespace(source_pointer, line, pos);   
            }
            catch(const LexerException& e)
            {
                free(source_copy);
                std::printf("Compilation failed\n");
                return;
            }
            
            if (ret.type != NONE)
                print_token(ret);
            clean_token(ret);
        }
        token eof = {END_OF_FILE, strdup(""), 0, 0};
        print_token(eof);
        clean_token(eof);
        
        free(source_copy);
        std::printf("Compilation succeeded: lexical analysis complete\n");
    }

#pragma endregion
    
    void destroy_token_list(std::vector<token>*& list)
    {
        for(token t : *list)
        {
            clean_token(t);
        }

        delete list;
    }
}