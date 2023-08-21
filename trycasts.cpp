#include "parser/parser.h"

#ifndef __TRYCASTS_CPP__
#define __TRYCASTS_CPP__

template<typename T, typename F>
bool tryCast(T* super, F*& sub)
{
    static_assert(std::is_base_of<T, F>::value, "cannot convert super class to an class that does not inherit from super class.");
    sub = dynamic_cast<F*>(super);
    return sub != nullptr;
}

template<typename F>
bool tryCastExpr (Parser::ExprNode*& super, F*& sub)
{
    return tryCast<Parser::ExprNode, F>(super, sub);
}

template<typename F>
bool tryCastCmd (Parser::CmdNode*& super, F*& sub)
{
    return tryCast<Parser::CmdNode, F>(super, sub);
}

template<typename F>
bool tryCastStmt(Parser::StmtNode*& super, F*& sub)
{
    return tryCast<Parser::StmtNode, F>(super, sub);
}

#endif