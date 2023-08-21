#include <cstring>
#include <cstdlib>
#include <math.h>
#include <errno.h>
#include "parser.h"

namespace Parser
{
    
    ParserException::ParserException(const std::string& m, const Lexer::token& t)
    {
        message = "\nEncountered Error at Parsing Step. Line " + std::to_string(t.line_number) + ",  Position " + std::to_string(t.char_numer) + ", Token Type " + Lexer::tokenTypeToString(t.type) + ".\n" + m;
    }

    ParserException::ParserException(const std::string& m)
    {
        message = "\nEncountered Error at Parsing Step.\n" + m;
    }

    const char* ParserException::what () const noexcept
    {
        return message.c_str();
    }

    template<class T>
    std::string NodeVectorToString(std::vector<std::unique_ptr<T>>& v)
    {
        std::string concat = "";

        for (int i = 0; i < v.size(); i++)
        {
            std::string terminator = " ";
            if (i == v.size() - 1)
                terminator = "";
            concat += v[i].get()->toString() + terminator;
        }

        return concat;
    }

    long castStringToInt(const std::string& text, Lexer::token errorLoc)
    {
        const char* begin = text.c_str();
        char* end; 

        long value = std::strtol(begin, &end, 10); // Cast to Int here

        if (errno == ERANGE)
            throw ParserException("\nInt was too big to parse. Tried to parse " + text + ".", errorLoc);
        
        if (isnan(value))
            throw ParserException("\nInt parsed as NaN. Tried to parse " + text + ".", errorLoc);

        return value;
    }

#pragma region ASTNode Implementations

    StringNode::StringNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::STRING);

        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }
    
    std::string StringNode::toString()
    {
        return token_s;
    }
    
    std::string StringNode::getValue()
    {
        return token_s.substr(1, token_s.length() - 2);
    }

#pragma region Command ASTNodes

    // read image <string> to <argument>
    ReadCmdNode::ReadCmdNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token readToken = consumeToken(token_index++, Lexer::READ);
        std::string readToken_s(readToken.text);
        std::string imageToken_s(consumeToken(token_index++, Lexer::IMAGE).text);
        std::unique_ptr<StringNode> _fileName(new StringNode());
        fileName = std::move(_fileName);
        std::string toToken_s(consumeToken(token_index++, Lexer::TO).text);
        std::unique_ptr<ArgumentNode> _readInto(parseArgument());
        readInto = std::move(_readInto);
        
        token_s = readToken_s + " " + imageToken_s + " " + fileName.get()->token_s + " " + toToken_s + " " + readInto.get()->token_s;
        line = readToken.line_number;
        pos = readToken.char_numer;
    }

    std::string ReadCmdNode::toString()
    {
        return "(ReadCmd " + fileName.get()->toString() + " " + readInto.get()->toString() + ")";
    }

    // write image <expr> to <string>
    WriteCmdNode::WriteCmdNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token writeToken = consumeToken(token_index++, Lexer::WRITE);
        std::string writeToken_s(writeToken.text);
        std::string imageToken_s(consumeToken(token_index++, Lexer::IMAGE).text);
        std::unique_ptr<ExprNode> _toSave(parseExpr());
        toSave = std::move(_toSave);
        std::string toToken_s(consumeToken(token_index++, Lexer::TO).text);
        std::unique_ptr<StringNode> _fileName(new StringNode());
        fileName = std::move(_fileName);
        
        token_s = writeToken_s + " " + imageToken_s + " " + toSave.get()->token_s + " " + toToken_s + " " + fileName.get()->token_s;
        line = writeToken.line_number;
        pos = writeToken.char_numer;
    }

    std::string WriteCmdNode::toString()
    {
        return "(WriteCmd " + toSave.get()->toString() + " " + fileName.get()->toString() + ")";
    }

    // type <variable> = <type>
    TypeCmdNode::TypeCmdNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token typeKeywordToken = consumeToken(token_index++, Lexer::TYPE);
        std::string typeKeywordToken_s(typeKeywordToken.text);
        std::string _variable(consumeToken(token_index++, Lexer::VARIABLE).text);
        variable = _variable;
        std::string equalToken_s(consumeToken(token_index++, Lexer::EQUALS).text);
        std::unique_ptr<TypeNode> _type(parseType());
        type = std::move(_type);
        
        token_s = typeKeywordToken_s + " " + variable + " " + equalToken_s + " " + type.get()->token_s;
        line = typeKeywordToken.line_number;
        pos = typeKeywordToken.char_numer;
    }

    std::string TypeCmdNode::toString()
    {
        return "(TypeCmd " + variable + " " + type.get()->toString() + ")"; 
    }

    // let <lvalue> = <expr>
    LetCmdNode::LetCmdNode(ASTNODE_CONSTRUCTOR_ARGS)
    {   
        Lexer::token letToken = consumeToken(token_index++, Lexer::LET);
        std::string letToken_s(letToken.text);
        std::unique_ptr<LValue> _lvalue(parseLValue());
        lvalue = std::move(_lvalue);
        std::string equalToken_s(consumeToken(token_index++, Lexer::EQUALS).text);
        std::unique_ptr<ExprNode> _expression(parseExpr());
        expression = std::move(_expression);
        
        token_s = letToken_s + " " + lvalue.get()->token_s + " " + equalToken_s + " " + expression.get()->token_s;
        line = letToken.line_number;
        pos = letToken.char_numer;
    }

    std::string LetCmdNode::toString()
    {
        return "(LetCmd " + lvalue.get()->toString() + " " + expression.get()->toString() + ")";
    }

    // assert <expr> , <string>
    AssertCmdNode::AssertCmdNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token assertToken = consumeToken(token_index++, Lexer::ASSERT);
        std::string assertToken_s(assertToken.text);
        std::unique_ptr<ExprNode> _expression(parseExpr());
        expression = std::move(_expression);
        std::string commaToken_s(consumeToken(token_index++, Lexer::COMMA).text);
        std::unique_ptr<StringNode> _string(new StringNode());
        string = std::move(_string);
        
        token_s = assertToken_s + " " + expression.get()->token_s + " " + commaToken_s + " " + string.get()->token_s;
        line = assertToken.line_number;
        pos = assertToken.char_numer;
    }

    std::string AssertCmdNode::toString()
    {
        return "(AssertCmd " + expression.get()->toString() + " " + string.get()->toString() + ")";
    }

    // print <string>
    PrintCmdNode::PrintCmdNode(ASTNODE_CONSTRUCTOR_ARGS)
    {   
        Lexer::token printToken = consumeToken(token_index++, Lexer::PRINT);
        std::string printToken_s(printToken.text);
        std::unique_ptr<StringNode> _string(new StringNode());
        string = std::move(_string);
        
        token_s = printToken_s + " " + string.get()->token_s;
        line = printToken.line_number;
        pos = printToken.char_numer;
    }

    std::string PrintCmdNode::toString()
    {
        return "(PrintCmd " + string.get()->toString() + ")";
    }

    // show <expr>
    ShowCmdNode::ShowCmdNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token showToken = consumeToken(token_index++, Lexer::SHOW);
        std::string showToken_s(showToken.text);
        std::unique_ptr<ExprNode> _expression(parseExpr());
        expression = std::move(_expression);
        
        token_s = showToken_s + " " + expression.get()->token_s;
        line = showToken.line_number;
        pos = showToken.char_numer;
    }

    std::string ShowCmdNode::toString()
    {
        return "(ShowCmd " + expression.get()->toString() + ")";
    }

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    
    // time <cmd>
    TimeCmdNode::TimeCmdNode()
    {
        Lexer::token time = consumeToken(token_index++, Lexer::TIME);
        token_s = "time ";
        line = time.line_number;
        pos = time.char_numer;

        CmdNode* cmd = parseCmd();
        token_s += cmd->token_s;
        std::unique_ptr<CmdNode> u_command(cmd);
        command = std::move(u_command);
    }
    std::string TimeCmdNode::toString()
    {
        return "(TimeCmd " + command.get()->toString() + ")";
    }

    // fn <variable> ( <binding> , ... ) : <type> { ;
    //     <stmt> ; ... ;
    // }
    FnCmd::FnCmd()
    {
        // fn
        {
            Lexer::token fn = consumeToken(token_index++, Lexer::FN);
            token_s = "fn ";
            line = fn.line_number;
            pos = fn.char_numer;
        }
        // <variable>
        {
            Lexer::token variable = consumeToken(token_index++, Lexer::VARIABLE);
            std::string variable_s(variable.text);
            token_s += variable_s + " ";
            function_name = variable_s;
        }
        //( <binding>, ... )
        {
            consumeToken(token_index++, Lexer::LPAREN);
            token_s += "(";

            while (peekToken(token_index) != Lexer::RPAREN)
            {
                std::unique_ptr<BindingNode> u_binding(parseBinding()); 
                
                token_s += " " + u_binding.get()->token_s;
                
                arguments.push_back(std::move(u_binding));
                
                if (peekToken(token_index) != Lexer::RPAREN)
                {
                    consumeToken(token_index++, Lexer::COMMA);
                    token_s += ",";
                }
            }

            consumeToken(token_index++, Lexer::RPAREN);
            token_s += " )";
        }
        // :
        {
            consumeToken(token_index++, Lexer::COLON);
            token_s += ": ";
        }
        // <type>
        {
            TypeNode* type = parseType();
            token_s += type->token_s + " ";
            std::unique_ptr<TypeNode> u_type(type);
            return_type = std::move(u_type);
        }
        // {; <stmt> ; ... ;}
        {
            consumeToken(token_index++, Lexer::LCURLY);
            consumeToken(token_index++, Lexer::NEWLINE);
            token_s += "{\n";

            while (peekToken(token_index) != Lexer::RCURLY)
            {
                std::unique_ptr<StmtNode> u_stmt(parseStmt()); 
                
                token_s += u_stmt.get()->token_s + "\n";
                
                function_contents.push_back(std::move(u_stmt));
                
                consumeToken(token_index++, Lexer::NEWLINE);
            }

            consumeToken(token_index++, Lexer::RCURLY);
            token_s += "}";
        }
    }
    std::string FnCmd::toString()
    {
        std::string arguments_str = NodeVectorToString<BindingNode>(arguments);
        std::string function_contents_str = NodeVectorToString<StmtNode>(function_contents);

        return "(FnCmd " + function_name + " (" + arguments_str + ") " + return_type.get()->toString() + " " + function_contents_str + ")";
    }
    

#pragma endregion

#pragma region Expression Nodes
    
    std::string ExprNode::rtypeToString()
    {
        if (resolvedType == nullptr)
            return "";
        
        return " (" + resolvedType.get()->toString() + ")";
    }

    IntExprNode::IntExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::INTVAL);

        std::string text(t.text);

        value = castStringToInt(text, t);

        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }
    
    std::string IntExprNode::toString()
    {
        return "(IntExpr" + rtypeToString() + " " + std::to_string(value) + ")";
    }
    
    FloatExprNode::FloatExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::FLOATVAL);

        std::string text(t.text);

        const char* begin = text.c_str();
        char* end;

        value = strtod(begin, &end); // Cast to Float here

        if (errno == ERANGE)
            throw ParserException("\nFloat was too big to parse. Tried to parse " + text + ".", t);
        
        if (isnan(value))
            throw ParserException("\nFloat parsed as NaN. Tried to parse " + text + ".", t);

        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }

    std::string FloatExprNode::toString()
    {
        return "(FloatExpr" + rtypeToString() + " " + std::to_string(static_cast<long>(value)) + ")";
    }

    TrueExprNode::TrueExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::TRUE);

        value = true;

        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }
    
    std::string TrueExprNode::toString()
    {
        return "(TrueExpr" + rtypeToString() + ")";
    }

    FalseExprNode::FalseExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::FALSE);

        value = false;

        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }
    
    std::string FalseExprNode::toString()
    {
        return "(FalseExpr" + rtypeToString() + ")";
    }

    VariableExprNode::VariableExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::VARIABLE);

        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }
    
    std::string VariableExprNode::toString()
    {
        return "(VarExpr" + rtypeToString() + " " + token_s + ")";
    }

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////
    
    // { <expr> , ... }
    TupleLiteralExprNode::TupleLiteralExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token t = consumeToken(token_index++, Lexer::LCURLY);
        
        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;

        while (peekToken(token_index) != Lexer::RCURLY)
        {
            std::unique_ptr<ExprNode> u_type(parseExpr()); 
            
            token_s += " " + u_type.get()->token_s;
            
            tuple_expressions.push_back(std::move(u_type));
            
            if (peekToken(token_index) != Lexer::RCURLY)
            {
                consumeToken(token_index++, Lexer::COMMA);
                token_s += ",";
            }
        }

        consumeToken(token_index++, Lexer::RCURLY);
        token_s += " }";
    }
    std::string TupleLiteralExprNode::toString()
    {
        return "(TupleLiteralExpr" + rtypeToString() + " " + NodeVectorToString<ExprNode>(tuple_expressions) + ")";
    }

    // [ <expr> , ... ]
    ArrayLiteralExprNode::ArrayLiteralExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token t = consumeToken(token_index++, Lexer::LSQUARE);
        
        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;

        while (peekToken(token_index) != Lexer::RSQUARE)
        {
            std::unique_ptr<ExprNode> u_type(parseExpr()); 
            
            token_s += " " + u_type.get()->token_s;
            
            array_expressions.push_back(std::move(u_type));
            
            if (peekToken(token_index) != Lexer::RSQUARE)
            {
                consumeToken(token_index++, Lexer::COMMA);
                token_s += ",";
            }
        }

        consumeToken(token_index++, Lexer::RSQUARE);
        token_s += " ]";
    }
    std::string ArrayLiteralExprNode::toString()
    {
        return "(ArrayLiteralExpr" + rtypeToString() + " " + NodeVectorToString<ExprNode>(array_expressions) + ")";
    }

    // <expr> { <integer> }
    TupleIndexExprNode::TupleIndexExprNode(ASTNODE_CONSTRUCTOR_ARGS ExprNode* head)
    {
        std::unique_ptr<ExprNode> u_head(head);

        token_s = head->token_s;
        line = head->line;
        pos = head->pos;

        tuple_expression = std::move(u_head);

        consumeToken(token_index++, Lexer::LCURLY);
        token_s += "{ ";
        const Lexer::token intval = consumeToken(token_index++, Lexer::INTVAL);
        std::string intstr(intval.text);
        tuple_index = castStringToInt(intstr, intval);
        token_s += std::to_string(tuple_index);
        consumeToken(token_index++, Lexer::RCURLY);
        token_s += " }";
    }
    std::string TupleIndexExprNode::toString()
    {
        return "(TupleIndexExpr" + rtypeToString() + " " + tuple_expression.get()->toString() + " " + std::to_string(tuple_index) + ")";
    }


    // <expr> [ <expr> , ... ]
    ArrayIndexExprNode::ArrayIndexExprNode(ASTNODE_CONSTRUCTOR_ARGS ExprNode* head)
    {
        std::unique_ptr<ExprNode> u_head(head);

        token_s = head->token_s;
        line = head->line;
        pos = head->pos;

        array_expression = std::move(u_head);

        consumeToken(token_index++, Lexer::LSQUARE);
        token_s += "[";

        while (peekToken(token_index) != Lexer::RSQUARE)
        {
            std::unique_ptr<ExprNode> u_type(parseExpr()); 
            
            token_s += " " + u_type.get()->token_s;
            
            array_indices.push_back(std::move(u_type));
            
            if (peekToken(token_index) != Lexer::RSQUARE)
            {
                consumeToken(token_index++, Lexer::COMMA);
                token_s += ",";
            }
        }

        consumeToken(token_index++, Lexer::RSQUARE);
        token_s += " ]";
    }
    std::string ArrayIndexExprNode::toString()
    {
        return "(ArrayIndexExpr" + rtypeToString() + " " + array_expression.get()->toString() + " " + NodeVectorToString<ExprNode>(array_indices) + ")";
    }

    // <variable> ( <expr> , ... )
    CallExprNode::CallExprNode(ASTNODE_CONSTRUCTOR_ARGS const Lexer::token& variable)
    {
        if (variable.type != Lexer::VARIABLE)
            throw ParserException("\nExpected a token of type VARIABLE; got a " + Lexer::tokenTypeToString(variable.type) + " instead.", variable);

        std::string text(variable.text);
        token_s = text;
        line = variable.line_number;
        pos = variable.char_numer;

        function_name = text;

        consumeToken(token_index++, Lexer::LPAREN);
        token_s += "(";

        while (peekToken(token_index) != Lexer::RPAREN)
        {
            std::unique_ptr<ExprNode> u_type(parseExpr()); 
            
            token_s += " " + u_type.get()->token_s;
            
            arguments.push_back(std::move(u_type));
            
            if (peekToken(token_index) != Lexer::RPAREN)
            {
                consumeToken(token_index++, Lexer::COMMA);
                token_s += ",";
            }
        }

        consumeToken(token_index++, Lexer::RPAREN);
        token_s += " )";
    }
    std::string CallExprNode::toString()
    {
        return "(CallExpr" + rtypeToString() + " " + function_name + " " + NodeVectorToString<ExprNode>(arguments) + ")";
    }
    
    ////////////////////////////////////////////
    //                  HW 5                  //
    ////////////////////////////////////////////

    // - <expr>
    // ! <expr>
    UnopExprNode::UnopExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token unop_token = consumeToken(token_index++, Lexer::OP);
        std::string text(unop_token.text);
        
        line = unop_token.line_number;
        pos = unop_token.char_numer;
        token_s = text + " ";

        operation = tokenToUnopType(unop_token);

        ExprNode* expr = parseUnopExpr();
        token_s += expr->token_s;
        std::unique_ptr<ExprNode> u_expr(expr);
        expression = std::move(u_expr);
    }

    std::string UnopExprNode::toString()
    {
        std::string s_operation(1, (char) operation); 
        return "(UnopExpr" + rtypeToString() + " " + s_operation + " " + expression.get()->toString() + ")";
    }

    UnopExprNode::UnopType UnopExprNode::tokenToUnopType(const Lexer::token& toConvert)
    {
        switch(toConvert.text[0])
        {
            case '-':
                return NEGATION;
            case '!':
                return NOT;
            default:
                throw ParserException(" Could not recognize character " + std::to_string(toConvert.text[0]) + " as a unary operator.", toConvert);
        }
    }

    BinopExprNode::BinopExprNode(ASTNODE_CONSTRUCTOR_ARGS std::unique_ptr<ExprNode>& _lhs, const Lexer::token& binop_token, std::unique_ptr<ExprNode>& _rhs)
    {
        line = _lhs.get()->line;
        pos = _lhs.get()->pos;
        token_s = _lhs.get()->token_s + " ";

        lhs = std::move(_lhs);
        
        std::string text(binop_token.text);
        token_s += text + " ";
        operation = tokenToBinopType(binop_token);

        token_s += _rhs.get()->token_s;
        rhs = std::move(_rhs);
    }

    std::string BinopExprNode::toString()
    {
        return "(BinopExpr" + rtypeToString() + " " + lhs.get()->toString() + " " + binopTypeToString(operation) + " " + rhs.get()->toString() + ")";
    }

    BinopExprNode::BinopType BinopExprNode::tokenToBinopType(const Lexer::token& toConvert)
    {
        std::string text(toConvert.text);

        if (text == "+")
            return PLUS;
        else if (text == "-")
            return MINUS;
        else if (text == "*")
            return TIMES;
        else if (text == "/")
            return DIVIDE;
        else if (text == "%")
            return MOD;
        else if (text == "<")
            return LESS_THAN;
        else if (text == ">")
            return GREATER_THAN;
        else if (text == "==")
            return EQUALS;
        else if (text == "!=")
            return NOT_EQUALS;
        else if (text == "<=")
            return LESS_THAN_OR_EQUALS;
        else if (text == ">=")
            return GREATER_THAN_OR_EQUALS;
        else if (text == "&&")
            return AND;
        else if (text == "||")
            return OR;
        
        throw ParserException("Could not convert " + text + " as a binary operator.", toConvert);
    }

    std::string BinopExprNode::binopTypeToString(const BinopType& toConvert)
    {
        switch(toConvert)
        {
            case PLUS:
                return "+";
            case MINUS:
                return "-";
            case TIMES:
                return "*";
            case DIVIDE:
                return "/";
            case MOD:
                return "%";
            case LESS_THAN:
                return "<";
            case GREATER_THAN:
                return ">";
            case EQUALS:
                return "==";
            case NOT_EQUALS:
                return "!=";
            case LESS_THAN_OR_EQUALS:
                return "<=";
            case GREATER_THAN_OR_EQUALS:
                return ">=";
            case AND:
                return "&&";
            case OR:
                return "||";
        }
    }

    // if <expr> then <expr> else <expr>
    IfExprNode::IfExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token& if_token = consumeToken(token_index++, Lexer::IF);
        
        line = if_token.line_number;
        pos = if_token.char_numer;
        token_s = "if ";

        ExprNode* _condition = parseExpr();
        token_s += _condition->token_s;
        std::unique_ptr<ExprNode> u_condition(_condition);
        condition = std::move(u_condition);

        consumeToken(token_index++, Lexer::THEN);
        token_s += " then ";

        ExprNode* _then_expr = parseExpr();
        token_s += _then_expr->token_s;
        std::unique_ptr<ExprNode> u_then_expr(_then_expr);
        then_expr = std::move(u_then_expr);

        consumeToken(token_index++, Lexer::ELSE);
        token_s += " else ";

        ExprNode* _else_expr = parseExpr();
        token_s += _else_expr->token_s;
        std::unique_ptr<ExprNode> u_else_expr(_else_expr);
        else_expr = std::move(u_else_expr);
    }

    std::string IfExprNode::toString()
    {
        return "(IfExpr" + rtypeToString() + " " + condition.get()->toString() + " " + then_expr.get()->toString() + " " + else_expr.get()->toString() + ")";
    }

    // [ <variable> : <expr> , ... ]
    std::vector<std::unique_ptr<std::pair<std::string, std::unique_ptr<ExprNode>>>> LoopExprNode::parseBounds()
    {

        std::vector<std::unique_ptr<std::pair<std::string, std::unique_ptr<ExprNode>>>> bounds;
        
        consumeToken(token_index++, Lexer::LSQUARE);
        token_s += " [";

        while (peekToken(token_index) != Lexer::RSQUARE)
        {
            const Lexer::token& var_token = consumeToken(token_index++, Lexer::VARIABLE);
            std::string var_name(var_token.text);

            consumeToken(token_index++, Lexer::COLON);

            std::unique_ptr<ExprNode> u_expr(parseExpr()); 
            
            token_s += " " + var_name + " : " + u_expr.get()->token_s;

            std::unique_ptr<std::pair<std::string, std::unique_ptr<ExprNode>>> u_bound(new std::pair(var_name, std::move(u_expr)));
            bounds.push_back(std::move(u_bound));
            
            if (peekToken(token_index) != Lexer::RSQUARE)
            {
                consumeToken(token_index++, Lexer::COMMA);
                token_s += ",";
                if (peekToken(token_index) == Lexer::RSQUARE)
                    throw ParserException(" Trailing comma detected.", consumeToken(--token_index, Lexer::COMMA));
            }

        }

        consumeToken(token_index++, Lexer::RSQUARE);
        token_s += " ]";

        return bounds;
    }

    std::string LoopExprNode::boundstoString(const std::vector<std::unique_ptr<std::pair<std::string, std::unique_ptr<ExprNode>>>>& bounds)
    {
        std::string bounds_s = "";

        for(const auto& bound : bounds)
        {
            bounds_s += bound.get()->first + " ";
            bounds_s += bound.get()->second.get()->toString() + " ";
        }

        return bounds_s;
    }

    // array [ <variable> : <expr> , ... ] <expr>
    ArrayLoopExprNode::ArrayLoopExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token& array_token = consumeToken(token_index++, Lexer::ARRAY);

        line = array_token.line_number;
        pos = array_token.char_numer;
        token_s = "array ";

        bounds = parseBounds();

        for(const auto& bound : bounds)
        {
            token_s += bound.get()->first;
            token_s += " " + bound.get()->second.get()->token_s + " ";
        }

        ExprNode* _loop_expression = parseExpr();
        token_s += _loop_expression->token_s;
        std::unique_ptr<ExprNode> u_loop_expression(_loop_expression);
        loop_expression = std::move(u_loop_expression);
    }

    std::string ArrayLoopExprNode::toString()
    {
        return "(ArrayLoopExpr" + rtypeToString() + " " + boundstoString(bounds) + loop_expression.get()->toString() + ")";
    }

    // sum [ <variable> : <expr> , ... ] <expr>
    SumLoopExprNode::SumLoopExprNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token& sum_token = consumeToken(token_index++, Lexer::SUM);

        line = sum_token.line_number;
        pos = sum_token.char_numer;
        token_s = "sum ";

        bounds = parseBounds();

        for(const auto& bound : bounds)
        {
            token_s += bound.get()->first;
            token_s += " " + bound.get()->second.get()->token_s + " ";
        }

        ExprNode* _loop_expression = parseExpr();
        token_s += _loop_expression->token_s;
        std::unique_ptr<ExprNode> u_loop_expression(_loop_expression);
        loop_expression = std::move(u_loop_expression);
    }

    std::string SumLoopExprNode::toString()
    {
        return "(SumLoopExpr" + rtypeToString() + " " + boundstoString(bounds) + loop_expression.get()->toString() + ")";
    }


#pragma endregion

#pragma region Type Nodes
    IntTypeNode::IntTypeNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::INT);

        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }

    std::string IntTypeNode::toString()
    {
        return "(IntType)";
    }

    BoolTypeNode::BoolTypeNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::BOOL);

        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }

    std::string BoolTypeNode::toString()
    {
        return "(BoolType)";
    }

    FloatTypeNode::FloatTypeNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::FLOAT);

        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }

    std::string FloatTypeNode::toString()
    {
        return "(FloatType)";
    }

    VariableTypeNode::VariableTypeNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::VARIABLE);

        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }

    std::string VariableTypeNode::toString()
    {
        return "(VarType " + token_s + ")";
    }

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    // <type> [ , ... ]
    ArrayTypeNode::ArrayTypeNode(ASTNODE_CONSTRUCTOR_ARGS TypeNode* head)
    {

        std::unique_ptr<TypeNode> u_head(head);
        array_type = std::move(u_head);

        line = head->line;
        pos = head->pos;
        token_s = head->token_s + "[";

        rank = 1;
        consumeToken(token_index++, Lexer::LSQUARE);

        while (peekToken(token_index) != Lexer::RSQUARE)
        {
            rank++;
            consumeToken(token_index++, Lexer::COMMA);
            token_s += ",";
        }

        consumeToken(token_index++, Lexer::RSQUARE);
        token_s += "]";
    }

    std::string ArrayTypeNode::toString()
    {
        return "(ArrayType " + array_type.get()->toString() + " " + std::to_string(rank)  + ")";
    }

    

    // { <type> , ... }
    TupleTypeNode::TupleTypeNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token lcurly = consumeToken(token_index++, Lexer::LCURLY);

        line = lcurly.line_number;
        pos = lcurly.char_numer;
        std::string text(lcurly.text);
        token_s = text;

        while (peekToken(token_index) != Lexer::RCURLY)
        {
            std::unique_ptr<TypeNode> u_type(parseType()); 
            
            token_s += " " + u_type.get()->token_s;
            
            tuple_types.push_back(std::move(u_type));
            
            if (peekToken(token_index) != Lexer::RCURLY)
            {
                consumeToken(token_index++, Lexer::COMMA);
                token_s += ",";
            }

        }

        consumeToken(token_index++, Lexer::RCURLY);
        token_s += "}";
    }
    
    std::string TupleTypeNode::toString()
    {
        return "(TupleType " + NodeVectorToString(tuple_types) + ")";
    }

#pragma endregion

#pragma region Argument Nodes

    VarArgumentNode::VarArgumentNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        const Lexer::token t = consumeToken(token_index++, Lexer::VARIABLE);

        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;
    }

    std::string VarArgumentNode::toString()
    {
        return "(VarArgument " + token_s + ")";
    }

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////
    
    // <variable> [ <variable> , ... ]
    ArrayArgumentNode::ArrayArgumentNode(ASTNODE_CONSTRUCTOR_ARGS const Lexer::token& variable)
    {

        if (variable.type != Lexer::VARIABLE)
            throw ParserException("\nExpected a token of type VARIABLE; got a " + Lexer::tokenTypeToString(variable.type) + " instead.", variable);

        std::string text(variable.text);
        token_s = text;
        line = variable.line_number;
        pos = variable.char_numer;
        
        array_argument_name = text;

        consumeToken(token_index++, Lexer::LSQUARE);
        token_s += "[";

        while (peekToken(token_index) != Lexer::RSQUARE)
        {
            Lexer::token dimension_var = consumeToken(token_index++, Lexer::VARIABLE); 
            
            std::string text(dimension_var.text);
            token_s += " " + text;
            
            array_dimensions_names.push_back(text);
            
            if (peekToken(token_index) != Lexer::RSQUARE)
            {
                consumeToken(token_index++, Lexer::COMMA);
                token_s += ",";
            }

        }

        consumeToken(token_index++, Lexer::RSQUARE);
        token_s += " ]";
    }

    std::string ArrayArgumentNode::toString()
    {
        std::string dimensions = "";

        for (int i = 0; i < array_dimensions_names.size(); i++)
        {
            std::string terminator = " ";
            if (i == array_dimensions_names.size() - 1)
                terminator = ""; 
            dimensions += array_dimensions_names[i] + terminator;
        }

        return "(ArrayArgument " + array_argument_name + " " + dimensions + ")";
    }
    
#pragma endregion

#pragma region LValue Nodes

    ArgumentLValue::ArgumentLValue(ASTNODE_CONSTRUCTOR_ARGS) : argument(parseArgument()) 
    {
        token_s = argument.get()->token_s;
        line = argument.get()->line;
        pos = argument.get()->pos;
    }
    
    std::string ArgumentLValue::toString()
    {
        return "(ArgLValue " + argument.get()->toString() + ")";
    }

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    
    // { <lvalue> , ... }
    TupleLValueNode::TupleLValueNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token t = consumeToken(token_index++, Lexer::LCURLY);
        
        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;

        while (peekToken(token_index) != Lexer::RCURLY)
        {
            std::unique_ptr<LValue> u_lvalue(parseLValue()); 
            
            token_s += " " + u_lvalue.get()->token_s;
            
            lvalues.push_back(std::move(u_lvalue));
            
            if (peekToken(token_index) != Lexer::RCURLY)
            {
                consumeToken(token_index++, Lexer::COMMA);
                token_s += ",";
            }

        }

        consumeToken(token_index++, Lexer::RCURLY);
        token_s += " }";
    }

    std::string TupleLValueNode::toString()
    {
        return "(TupleLValue " + NodeVectorToString(lvalues) + ")";
    }
    
#pragma endregion

#pragma region Binding Nodes
    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    
    // { <binding> , ... }
    TupleBindingNode::TupleBindingNode(ASTNODE_CONSTRUCTOR_ARGS)
    {
        Lexer::token t = consumeToken(token_index++, Lexer::LCURLY);
        
        std::string text(t.text);
        token_s = text;
        line = t.line_number;
        pos = t.char_numer;

        while (peekToken(token_index) != Lexer::RCURLY)
        {
            std::unique_ptr<BindingNode> u_binding(parseBinding()); 
            
            token_s += " " + u_binding.get()->token_s;
            
            bindings.push_back(std::move(u_binding));
            
            if (peekToken(token_index) != Lexer::RCURLY)
            {
                consumeToken(token_index++, Lexer::COMMA);
                token_s += ",";
            }

        }

        consumeToken(token_index++, Lexer::RCURLY);
        token_s += " }";
    }

    std::string TupleBindingNode::toString()
    {
        return "(TupleBinding " + NodeVectorToString(bindings) + ")";
    }

    // <argument> : <type>
    VarBindingNode::VarBindingNode()
    {
        ArgumentNode* arg = parseArgument();

        line = arg->line;
        pos = arg->pos;
        token_s = arg->token_s + " ";

        std::unique_ptr<ArgumentNode> u_arg(arg);
        argument = std::move(u_arg);

        consumeToken(token_index++, Lexer::COLON);
        token_s += ": ";

        TypeNode* t = parseType();
        token_s += t->token_s;
        std::unique_ptr<TypeNode> u_type(t);
        type = std::move(u_type);
    }

    std::string VarBindingNode::toString()
    {
        return "(VarBinding " + argument.get()->toString() + " " + type.get()->toString() + ")";
    }
    

#pragma endregion

#pragma region Statement Nodes
    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    
    // let <lvalue> = <expr>
    LetStmtNode::LetStmtNode(ASTNODE_CONSTRUCTOR_ARGS)
    {

        Lexer::token let_t = consumeToken(token_index++, Lexer::LET);
        std::string text(let_t.text);

        token_s = text + " ";
        line = let_t.line_number;
        pos = let_t.char_numer;

        LValue* lvalue = parseLValue();
        token_s += lvalue->token_s + " ";
        std::unique_ptr<LValue> u_lvalue(lvalue);
        set_variable_name = std::move(u_lvalue);


        consumeToken(token_index++, Lexer::EQUALS);
        token_s += "= ";

        ExprNode* expr = parseExpr();
        token_s += expr->token_s;
        std::unique_ptr<ExprNode> u_expr(expr);
        variable_expression = std::move(u_expr);
    }
    std::string LetStmtNode::toString()
    {
        return "(LetStmt " + set_variable_name.get()->toString() + " " + variable_expression.get()->toString() + ")";
    }

    // assert <expr> , <string>
    AssertStmtNode::AssertStmtNode()
    {
        
        Lexer::token assertToken = consumeToken(token_index++, Lexer::ASSERT);
        std::string assertToken_s(assertToken.text);
        std::unique_ptr<ExprNode> _expression(parseExpr());
        expression = std::move(_expression);
        std::string commaToken_s(consumeToken(token_index++, Lexer::COMMA).text);
        std::unique_ptr<StringNode> _string(new StringNode());
        string = std::move(_string);
        
        token_s = assertToken_s + " " + expression.get()->token_s + " " + commaToken_s + " " + string.get()->token_s;
        line = assertToken.line_number;
        pos = assertToken.char_numer;
    }    
    std::string AssertStmtNode::toString()
    {
        return "(AssertStmt " + expression.get()->toString() + " " + string.get()->toString() + ")";
    }

    // return <expr>
    ReturnStmtNode::ReturnStmtNode()
    {

        Lexer::token return_t = consumeToken(token_index++, Lexer::RETURN);
        std::string text(return_t.text);

        token_s = text + " ";
        line = return_t.line_number;
        pos = return_t.char_numer;

        ExprNode* expr = parseExpr();
        token_s += expr->token_s;
        std::unique_ptr<ExprNode> u_expr(expr);
        expression = std::move(u_expr);
    }
    std::string ReturnStmtNode::toString()
    {
        return "(ReturnStmt " + expression.get()->toString() + ")";
    }
    
#pragma endregion

#pragma endregion

#pragma region Parser Implementation
    std::vector<std::unique_ptr<CmdNode>> parse(std::vector<Lexer::token>* _tokens) 
    {
        tokens = _tokens;
        return parseAllTokens();
    }

    std::string parseToString(std::vector<Lexer::token>* _tokens)
    {
        std::vector<std::unique_ptr<CmdNode>> tree;

        try
        {
            tree = parse(_tokens);
        }
        catch(const std::exception& e)
        {
            return "Compilation failed\n";
        }

        std::string message = "";

        for (int i = 0; i < tree.size(); i++)
        {
            message += tree[i].get()->toString() + "\n";
        }

        return message + "Compilation succeeded\n";
    }

    Lexer::TokenType peekToken(int index_to_peek)
    {
        if (index_to_peek < 0)
            throw ParserException("\nAsked to see a token at index < 0 " + std::to_string(index_to_peek) + ".");
        
        if (index_to_peek >= (*tokens).size())
            throw ParserException("\nTried to query a token when there were none left; # of tokens: " + std::to_string((*tokens).size()) + ", index to peek " + std::to_string(index_to_peek) + ".");

        return (*tokens)[index_to_peek].type;
    }

    const Lexer::token consumeToken(int index_to_consume, Lexer::TokenType expected_token_type)
    {
        if (index_to_consume < 0)
            throw ParserException("\nExpected to see a " + Lexer::tokenTypeToString(expected_token_type) +  " token at index < 0 " + std::to_string(expected_token_type) + ".");
        
        if (index_to_consume >= (*tokens).size())
            throw ParserException("\nExpected to see a " + Lexer::tokenTypeToString(expected_token_type) + " token when there were none left; # of tokens: " + std::to_string((*tokens).size()) + ", index to consume " + std::to_string(expected_token_type) + ".");

        Lexer::token token_of_interest = (*tokens)[index_to_consume];

        if (token_of_interest.type != expected_token_type)
            throw ParserException("\nExpected token of type " + Lexer::tokenTypeToString(expected_token_type) + ", but got a token of type " + Lexer::tokenTypeToString(token_of_interest.type) + "." , token_of_interest);
        
        return token_of_interest;
    }

    std::vector<std::unique_ptr<CmdNode>> parseAllTokens()
    {
        std::vector<std::unique_ptr<CmdNode>> treeNodes;

        if (peekToken(token_index) == Lexer::NEWLINE)
            consumeToken(token_index++, Lexer::NEWLINE);

        while (peekToken(token_index) != Lexer::END_OF_FILE)
        {
            std::unique_ptr<CmdNode> unique_ast(parseCmd()); 
            treeNodes.push_back(std::move(unique_ast));
            consumeToken(token_index++, Lexer::NEWLINE);
        }
        

        return treeNodes;
    }

    CmdNode* parseCmd()
    {
        Lexer::TokenType tokenType = peekToken(token_index);

        switch(tokenType)
        {
            case Lexer::READ:
                return new ReadCmdNode();
            case Lexer::WRITE:
                return new WriteCmdNode();
            case Lexer::TYPE:
                return new TypeCmdNode();
            case Lexer::LET:
                return new LetCmdNode();
            case Lexer::ASSERT:
                return new AssertCmdNode();
            case Lexer::PRINT:
                return new PrintCmdNode();
            case Lexer::SHOW:
                return new ShowCmdNode();
            case Lexer::TIME:
                return new TimeCmdNode();
            case Lexer::FN:
                return new FnCmd();
            default:
                throw ParserException("\nFailed to parse a command; got a " + Lexer::tokenTypeToString(tokenType) + " token instead.", (*tokens)[token_index]);
        }
    }

    TypeNode* parseType()
    {
        
        TypeNode* head = parseTypeHead();
        TypeNode* full = parseTypeCont(head);
        return full;
    }

    TypeNode* parseTypeHead()
    {
        Lexer::TokenType tokenType = peekToken(token_index);

        switch(tokenType)
        {
            case Lexer::INT:
                return new IntTypeNode();
            case Lexer::BOOL:
                return new BoolTypeNode();
            case Lexer::FLOAT:
                return new FloatTypeNode();
            case Lexer::VARIABLE:
                return new VariableTypeNode();
            case Lexer::LCURLY:
                return new TupleTypeNode();
            default:
                throw ParserException("\nFailed to parse a type; got a " + Lexer::tokenTypeToString(tokenType) + " token instead.", (*tokens)[token_index]);
        }
    }

    TypeNode* parseTypeCont(TypeNode* t)
    {
        Lexer::TokenType tokenType = peekToken(token_index);

        switch(tokenType)
        {
            case Lexer::LSQUARE:
                return parseTypeCont(new ArrayTypeNode(t));
            default:
                return t;
        }
    }

    ExprNode* parseExpr()
    {
        return parseBoolopExpr();
    }

    ExprNode* parseBoolopExpr()
    {
        std::unique_ptr<ExprNode> head(parseComparisonExpr());
        return parseBoolopExprCont(head);
    }

    ExprNode* parseBoolopExprCont(std::unique_ptr<ExprNode>& head)
    {
        Lexer::TokenType tokenType = peekToken(token_index);
        
        if (tokenType != Lexer::OP)
            return head.release();
        
        const Lexer::token& binop_token = consumeToken(token_index++, Lexer::OP);
        std::string text(binop_token.text);

        bool is_boolop = text == "&&" || text == "||";
        
        if (!is_boolop)
        {
            token_index--;
            return head.release();
        }

        std::unique_ptr<ExprNode> _rhs(parseComparisonExpr());
        std::unique_ptr<ExprNode> fullexpr(new BinopExprNode(head, binop_token, _rhs));
        return parseBoolopExprCont(fullexpr);
    }

    ExprNode* parseComparisonExpr()
    {
        std::unique_ptr<ExprNode> head(parseAddExpr());
        return parseComparisonExprCont(head);
    }

    ExprNode* parseComparisonExprCont(std::unique_ptr<ExprNode>& head)
    {
        Lexer::TokenType tokenType = peekToken(token_index);
        
        if (tokenType != Lexer::OP)
            return head.release();
        
        const Lexer::token& binop_token = consumeToken(token_index++, Lexer::OP);
        std::string text(binop_token.text);

        bool is_lt = text == "<" || text == "<=";
        bool is_gt = text == ">" || text == ">=";
        bool is_equ =  text == "==" || text == "!=";
        
        if ( !(is_lt || is_gt || is_equ))
        {
            token_index--;
            return head.release();
        }

        std::unique_ptr<ExprNode> _rhs(parseAddExpr());
        std::unique_ptr<ExprNode> fullexpr(new BinopExprNode(head, binop_token, _rhs));
        return parseComparisonExprCont(fullexpr);
    }

    ExprNode* parseAddExpr()
    {
        std::unique_ptr<ExprNode> head(parseMultExpr());
        return parseAddExprCont(head);
    }

    ExprNode* parseAddExprCont(std::unique_ptr<ExprNode>& head)
    {
        Lexer::TokenType tokenType = peekToken(token_index);
        
        if (tokenType != Lexer::OP)
            return head.release();
        
        const Lexer::token& binop_token = consumeToken(token_index++, Lexer::OP);
        switch (binop_token.text[0])
        {
        case '+':
        case '-':
            {
                std::unique_ptr<ExprNode> _rhs(parseMultExpr());
                std::unique_ptr<ExprNode> fullexpr(new BinopExprNode(head, binop_token, _rhs));
                return parseAddExprCont(fullexpr);
            }   
        default:
            token_index--;
            return head.release();
        }
    }

    ExprNode* parseMultExpr()
    {
        std::unique_ptr<ExprNode> head(parseUnopExpr());
        return parseMultExprCont(head);
    }

    ExprNode* parseMultExprCont(std::unique_ptr<ExprNode>& head)
    {
        Lexer::TokenType tokenType = peekToken(token_index);
        
        if (tokenType != Lexer::OP)
            return head.release();
        
        const Lexer::token& binop_token = consumeToken(token_index++, Lexer::OP);
        switch (binop_token.text[0])
        {
        case '*':
        case '/':
        case '%':
            {
                std::unique_ptr<ExprNode> _rhs(parseUnopExpr());
                std::unique_ptr<ExprNode> fullexpr(new BinopExprNode(head, binop_token, _rhs));
                return parseMultExprCont(fullexpr);
            }   
        default:
            token_index--;
            return head.release();
        }
    }

    ExprNode* parseUnopExpr()
    {
        Lexer::TokenType tokenType = peekToken(token_index);
        
        if( tokenType != Lexer::OP)
            return parseBaseExpr();

        return new UnopExprNode();
    }

    ExprNode* parseBaseExpr()
    {
        ExprNode* head = parseBaseExprHead();
        ExprNode* full = parseBaseExprCont(head);
        return full;
    }

    ExprNode* parseBaseExprHead()
    {
        Lexer::TokenType tokenType = peekToken(token_index);

        switch(tokenType)
        {
            case Lexer::INTVAL: // int expr
                return new IntExprNode();
            case Lexer::FLOATVAL: // float expr
                return new FloatExprNode();
            case Lexer::TRUE: // true expr
                return new TrueExprNode();
            case Lexer::FALSE: // false expr
                return new FalseExprNode();
            case Lexer::VARIABLE: // var expr or call expr
                return parseBaseExprVarCont(consumeToken(token_index++, Lexer::VARIABLE));
            case Lexer::LPAREN: // ( <expr> )
                {
                    consumeToken(token_index++, Lexer::LPAREN);
                    ExprNode* expr = parseExpr();
                    consumeToken(token_index++, Lexer::RPAREN);
                    return expr;
                }
            case Lexer::LCURLY:
                return new TupleLiteralExprNode();
            case Lexer::LSQUARE:
                return new ArrayLiteralExprNode();
            case Lexer::IF:
                return new IfExprNode();
            case Lexer::ARRAY:
                return new ArrayLoopExprNode();
            case Lexer::SUM:
                return new SumLoopExprNode();
            default:
                throw ParserException("\nFailed to parse an expression; got a " + Lexer::tokenTypeToString(tokenType) + " token instead.", (*tokens)[token_index]);
        }
    }

    ExprNode* parseBaseExprVarCont(const Lexer::token& variable)
    {
        if (variable.type != Lexer::VARIABLE)
            throw ParserException("\nExpected a token of type VARIABLE; got a " + Lexer::tokenTypeToString(variable.type) + " instead.", variable);
        
        Lexer::TokenType tokenType = peekToken(token_index);
        
        switch(tokenType)
        {
            case Lexer::LPAREN:
                return new CallExprNode(variable);
            default:
                token_index--;
                return new VariableExprNode();
        }
    }

    ExprNode* parseBaseExprCont(ExprNode* head)
    {
        Lexer::TokenType tokenType = peekToken(token_index);

        switch(tokenType)
        {
            case Lexer::LCURLY:
                return parseBaseExprCont(new TupleIndexExprNode(head));
            case Lexer::LSQUARE:
                return parseBaseExprCont(new ArrayIndexExprNode(head));
            default:
                return head;
        }
    }

    ArgumentNode* parseArgument()
    {
        const Lexer::token variable = consumeToken(token_index++, Lexer::VARIABLE);
        
        Lexer::TokenType tokenType = peekToken(token_index);

        switch(tokenType)
        {
            case Lexer::LSQUARE:
                return new ArrayArgumentNode(variable);
            default:
                token_index--;
                return new VarArgumentNode();
        }
    }

    LValue* parseLValue()
    {
        Lexer::TokenType tokenType = peekToken(token_index);

        switch(tokenType)
        {       
            case Lexer::LCURLY:
                return new TupleLValueNode();
            default:
                return new ArgumentLValue();
        }
    }

    BindingNode* parseBinding()
    {
        Lexer::TokenType tokenType = peekToken(token_index);

        switch(tokenType)
        {
            case Lexer::VARIABLE:
                return new VarBindingNode();
            case Lexer::LCURLY:
                return new TupleBindingNode();
            default:
                throw ParserException("\nFailed to parse a binding; got a " + Lexer::tokenTypeToString(tokenType) + " token instead.", (*tokens)[token_index]);
        }
    }

    StmtNode* parseStmt()
    {
        Lexer::TokenType tokenType = peekToken(token_index);

        switch (tokenType)
        {
            case Lexer::LET:
                return new LetStmtNode();
            case Lexer::ASSERT:
                return new AssertStmtNode();
            case Lexer::RETURN:
                return new ReturnStmtNode();
            default:
                throw ParserException("Failed to parse a statement; got a " + Lexer::tokenTypeToString(tokenType) + " token instead.", (*tokens)[token_index]);
        }
    }
#pragma endregion
}