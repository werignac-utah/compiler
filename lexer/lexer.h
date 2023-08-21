#include <vector>
#include <string>

#define LEX_FUNC_ARGS_NAMED char* &source, unsigned long& line, unsigned long& pos 
#define LEX_FUNC_ARGS char*&, unsigned long&, unsigned long&

#ifndef __LEXER_H__
#define __LEXER_H__ 

namespace Lexer
{
    ///////////////////////////////////////////////////
    //                  Lexer Types                  //
    ///////////////////////////////////////////////////
    enum TokenType {ARRAY, ASSERT, BOOL, ELSE, FALSE, FLOAT, FN, IF, IMAGE, INT, LET, PRINT,
    READ, RETURN, SHOW, SUM, THEN, TIME, TO, TRUE, TYPE, WRITE,
    COLON, LCURLY, RCURLY, LPAREN, RPAREN, COMMA, LSQUARE, RSQUARE, EQUALS,
    STRING, INTVAL, FLOATVAL, VARIABLE, OP, NEWLINE, END_OF_FILE, NONE};

    typedef struct token {
        TokenType type;
        char* text;
        unsigned long line_number;
        unsigned long char_numer;
    } token;

    class LexerException : public std::exception 
    {
        public:
            std::string message;
            LexerException(const std::string& m, unsigned long line, unsigned long pos, const char* source);
            const char* what () const noexcept override;
    };

    ///////////////////////////////////////////////////////
    //                  Lexer Functions                  //
    ///////////////////////////////////////////////////////

    std::vector<token>* lexAll(const char* source);
    void lexPrintAll(const char* source);
    token lexToken(LEX_FUNC_ARGS_NAMED);
    std::string tokenTypeToString(TokenType to_convert);
    void destroy_token_list(std::vector<token>*&);
}

#endif