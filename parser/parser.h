#include <string>
#include <memory>
#include <utility>
#include "../lexer/lexer.h"
#include "../typechecker/types.h"

#ifndef __PARSER_H__
#define __PARSER_H__

#define ASTNODE_CONSTRUCTOR_ARGS

namespace Parser
{
    class ParserException : public std::exception 
    {
        public:
            std::string message;
            ParserException(const std::string& m, const Lexer::token& t);
            ParserException(const std::string&m);
            const char* what () const noexcept override;
    };

#pragma region ASTNodes

#pragma region Optimization Helpers

    typedef struct CPValue
    {
        enum CPType {NONE, INT, ARRAY};

    public:
        CPType type = NONE;

    } CPValue;

    typedef struct IntValue : public CPValue
    {
    public:
        long value;
        IntValue(const long _value) : value(_value) {type = INT;}
    } IntValue;

    typedef struct ArrayValue : public CPValue
    {
    public:
        std::vector<std::shared_ptr<CPValue>> lengths;
        ArrayValue(std::vector<std::shared_ptr<CPValue>>& _lengths) : lengths(_lengths) {type = ARRAY;} 
    } ArrayValue;

#pragma endregion

#pragma region Root Nodes

    class ASTNode {
        public:
            virtual std::string toString() = 0;
            virtual ~ASTNode() {};

            std::string token_s;
            unsigned long line;
            unsigned long pos;
    };

    class StringNode: public ASTNode
    {
        public:
            StringNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~StringNode() {};
            std::string getValue();
    };

    class ArgumentNode: public ASTNode
    {
        public:
            virtual ~ArgumentNode() {};
    };

    class LValue: public ASTNode
    {
        public:
            virtual ~LValue() {};
    };

    class BindingNode: public ASTNode
    {
        public:
            virtual ~BindingNode() {};
    };

    class ExprNode: public ASTNode 
    {
        public:
            std::shared_ptr<CPValue> cp;

            virtual ~ExprNode() {};
            mutable std::shared_ptr<Typechecker::ResolvedType> resolvedType = nullptr;
        protected:
            ExprNode() {cp = std::make_shared<CPValue>();}
            std::string rtypeToString();
    };

    class StmtNode: public ASTNode
    {
        public:
            virtual ~StmtNode() {};
    };

    class TypeNode: public ASTNode 
    {
        public: 
            virtual ~TypeNode() {};
    };

    class CmdNode : public ASTNode
    {
        public:
            virtual ~CmdNode() {};
    };

#pragma endregion

#pragma region Argument ASTNodes

    class VarArgumentNode : public ArgumentNode
    {
        public:
            VarArgumentNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~VarArgumentNode() {};
    };

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    
    // <variable> [ <variable> , ... ]
    class ArrayArgumentNode: public ArgumentNode
    {
        public:
            ArrayArgumentNode(ASTNODE_CONSTRUCTOR_ARGS const Lexer::token& variable);
            virtual std::string toString();
            virtual ~ArrayArgumentNode() {};
            std::string array_argument_name;
            std::vector<std::string> array_dimensions_names;
    };
    

#pragma endregion

#pragma region Binding ASTNodes

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    
    // { <binding> , ... }
    class TupleBindingNode: public BindingNode
    {
        public:
            TupleBindingNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~TupleBindingNode() {};
            std::vector<std::unique_ptr<BindingNode>> bindings;
    };

    // <argument> : <type>
    class VarBindingNode: public BindingNode
    {
        public:
            VarBindingNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~VarBindingNode() {};
            std::unique_ptr<ArgumentNode> argument;
            std::unique_ptr<TypeNode> type;
    };

#pragma endregion

#pragma region LValue ASTNodes

    class ArgumentLValue: public LValue
    {
        public:
            ArgumentLValue(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~ArgumentLValue() {};
            std::unique_ptr<ArgumentNode> argument;
    };

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    // { <lvalue> , ... }
    class TupleLValueNode: public LValue
    {
        public:
            TupleLValueNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~TupleLValueNode() {};
            std::vector<std::unique_ptr<LValue>> lvalues;
    };
    

#pragma endregion

#pragma region Expression ASTNodes

    class IntExprNode : public ExprNode
    {
        public:
            IntExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~IntExprNode() {};
            long value;
    };

    class FloatExprNode : public ExprNode
    {
        public:
            FloatExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~FloatExprNode() {};
            double value;
    };

    class TrueExprNode : public ExprNode
    {
        public:
            TrueExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~TrueExprNode() {};
            bool value;
    };

    class FalseExprNode : public ExprNode
    {
        public:
            FalseExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~FalseExprNode() {};
            bool value;
    };

    class VariableExprNode : public ExprNode
    {
        public:
            VariableExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~VariableExprNode() {};
    };

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    
    // { <expr> , ... }
    class TupleLiteralExprNode: public ExprNode
    {
        public:
            TupleLiteralExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~TupleLiteralExprNode() {};
            std::vector<std::unique_ptr<ExprNode>> tuple_expressions;
    };

    // [ <expr> , ... ]
    class ArrayLiteralExprNode: public ExprNode
    {
        public:
            ArrayLiteralExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~ArrayLiteralExprNode() {};
            std::vector<std::unique_ptr<ExprNode>> array_expressions;
    };

    // <expr> { <integer> }
    class TupleIndexExprNode: public ExprNode
    {
        public:
            TupleIndexExprNode(ASTNODE_CONSTRUCTOR_ARGS ExprNode* head);
            virtual std::string toString();
            virtual ~TupleIndexExprNode() {};
            std::unique_ptr<ExprNode> tuple_expression;
            long tuple_index;
    };

    // <expr> [ <expr> , ... ]
    class ArrayIndexExprNode: public ExprNode
    {
        public:
            ArrayIndexExprNode(ASTNODE_CONSTRUCTOR_ARGS ExprNode* head);
            virtual std::string toString();
            virtual ~ArrayIndexExprNode() {};
            std::unique_ptr<ExprNode> array_expression;
            std::vector<std::unique_ptr<ExprNode>> array_indices;
    };

    // <variable> ( <expr> , ... )
    class CallExprNode: public ExprNode
    {
        public:
            CallExprNode(ASTNODE_CONSTRUCTOR_ARGS const Lexer::token& variable);
            virtual std::string toString();
            virtual ~CallExprNode() {};
            std::string function_name;
            std::vector<std::unique_ptr<ExprNode>> arguments;
    };

    ////////////////////////////////////////////
    //                  HW 5                  //
    ////////////////////////////////////////////

    // - <expr>
    // ! <expr>
    class UnopExprNode: public ExprNode
    {

        public:
            UnopExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~UnopExprNode() {};

            enum UnopType {
                NEGATION = '-',
                NOT = '!'
            };

            UnopType operation;
            std::unique_ptr<ExprNode> expression;
        
        private:
            static UnopType tokenToUnopType(const Lexer::token& toConvert);
    };
    
    
    class BinopExprNode: public ExprNode
    {
        public:
            BinopExprNode(ASTNODE_CONSTRUCTOR_ARGS std::unique_ptr<ExprNode>& _lhs, const Lexer::token& binop_token, std::unique_ptr<ExprNode>& _rhs);
            virtual std::string toString();
            virtual ~BinopExprNode() {};

            enum BinopType{
                PLUS,
                MINUS,
                TIMES,
                DIVIDE,
                MOD,
                LESS_THAN,
                GREATER_THAN,
                EQUALS,
                NOT_EQUALS,
                LESS_THAN_OR_EQUALS,
                GREATER_THAN_OR_EQUALS,
                AND,
                OR
            };

            std::unique_ptr<ExprNode> lhs;
            BinopType operation;
            std::unique_ptr<ExprNode> rhs;
        
        private:
            static BinopType tokenToBinopType(const Lexer::token& toConvert);
            static std::string binopTypeToString(const BinopType& toConvert);
    };
    
    
    class IfExprNode: public ExprNode
    {
        public:
            IfExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~IfExprNode() {};

            std::unique_ptr<ExprNode> condition;
            std::unique_ptr<ExprNode> then_expr;
            std::unique_ptr<ExprNode> else_expr;
    };

    
    class LoopExprNode: public ExprNode
    {
        public:
            virtual ~LoopExprNode() {};
            std::vector<std::unique_ptr<std::pair<std::string, std::unique_ptr<ExprNode>>>> bounds;
            std::unique_ptr<ExprNode> loop_expression;
        protected:
            std::vector<std::unique_ptr<std::pair<std::string, std::unique_ptr<ExprNode>>>> parseBounds();
            static std::string boundstoString(const std::vector<std::unique_ptr<std::pair<std::string, std::unique_ptr<ExprNode>>>>& bounds); 
    };

    // array [ <variable> : <expr> , ... ] <expr>
    class ArrayLoopExprNode: public LoopExprNode
    {
        public:
            ArrayLoopExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~ArrayLoopExprNode() {};
    };

    // sum [ <variable> : <expr> , ... ] <expr>
    class SumLoopExprNode: public LoopExprNode
    {
        public:
            SumLoopExprNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~SumLoopExprNode() {};
    };
    
#pragma endregion

#pragma region Statement ASTNodes

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    
    // let <lvalue> = <expr>
    class LetStmtNode: public StmtNode
    {
        public:
            LetStmtNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~LetStmtNode() {};
            std::unique_ptr<LValue> set_variable_name;
            std::unique_ptr<ExprNode> variable_expression;
    };

    // assert <expr> , <string>
    class AssertStmtNode: public StmtNode
    {
        public:
            AssertStmtNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~AssertStmtNode() {};
            std::unique_ptr<ExprNode> expression;
            std::unique_ptr<StringNode> string;
    };

    // return <expr>
    class ReturnStmtNode: public StmtNode
    {
        public:
            ReturnStmtNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~ReturnStmtNode() {};
            std::unique_ptr<ExprNode> expression;
    };
    

#pragma endregion

#pragma region Type ASTNodes

    class IntTypeNode: public TypeNode {
        public:
            IntTypeNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~IntTypeNode() {};
    };

    class BoolTypeNode: public TypeNode {
        public:
            BoolTypeNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~BoolTypeNode() {};
    };

    class FloatTypeNode: public TypeNode {
        public:
            FloatTypeNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~FloatTypeNode() {};
    };

    class VariableTypeNode: public TypeNode {
        public:
            VariableTypeNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~VariableTypeNode() {};
    };

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    // <type> [ , ... ]
    class ArrayTypeNode: public TypeNode {
        public:
            ArrayTypeNode(ASTNODE_CONSTRUCTOR_ARGS TypeNode* head);
            virtual std::string toString();
            virtual ~ArrayTypeNode() {};
            std::unique_ptr<TypeNode> array_type;
            int rank;
    };

    // { <type> , ... }
    class TupleTypeNode: public TypeNode {
        public:
            TupleTypeNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~TupleTypeNode() {};
            std::vector<std::unique_ptr<TypeNode>> tuple_types;
    };

#pragma endregion   

#pragma region Command ASTNodes

    // ReadCmd WriteCmd TypeCmd LetCmd AssertCmd PrintCmd ShowCmd
    class ReadCmdNode : public CmdNode
    {
        public:
            ReadCmdNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~ReadCmdNode() {};
            std::unique_ptr<StringNode> fileName;
            std::unique_ptr<ArgumentNode> readInto;
    };

    class WriteCmdNode : public CmdNode
    {
        public:
            WriteCmdNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~WriteCmdNode() {};
            std::unique_ptr<ExprNode> toSave;
            std::unique_ptr<StringNode> fileName;
    };

    class TypeCmdNode : public CmdNode
    {
        public:
            TypeCmdNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~TypeCmdNode() {};
            std::string variable;
            std::unique_ptr<TypeNode> type;
    };

    class LetCmdNode : public CmdNode
    {
        public:
            LetCmdNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~LetCmdNode() {};
            std::unique_ptr<LValue> lvalue;
            std::unique_ptr<ExprNode> expression;
    };

    class AssertCmdNode : public CmdNode
    {
        public:
            AssertCmdNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~AssertCmdNode() {};
            std::unique_ptr<ExprNode> expression;
            std::unique_ptr<StringNode> string;
    };

    class PrintCmdNode : public CmdNode
    {
        public:
            PrintCmdNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~PrintCmdNode() {};
            std::unique_ptr<StringNode> string;
    };

    class ShowCmdNode : public CmdNode
    {
        public:
            ShowCmdNode(ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~ShowCmdNode() {};
            std::unique_ptr<ExprNode> expression;
    };

    ////////////////////////////////////////////
    //                  HW 4                  //
    ////////////////////////////////////////////

    
    // time <cmd>
    class TimeCmdNode: public CmdNode
    {
        public:
            TimeCmdNode (ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~TimeCmdNode() {};
            std::unique_ptr<CmdNode> command;
    };

    // fn <variable> ( <binding> , ... ) : <type> { ;
    //     <stmt> ; ... ;
    // }
    class FnCmd: public CmdNode
    {
        public:
            FnCmd (ASTNODE_CONSTRUCTOR_ARGS);
            virtual std::string toString();
            virtual ~FnCmd() {};
            std::string function_name;
            std::vector<std::unique_ptr<BindingNode>> arguments;
            std::unique_ptr<TypeNode> return_type;
            std::vector<std::unique_ptr<StmtNode>> function_contents;
            std::shared_ptr<void> scope;
    };
    

#pragma endregion

#pragma endregion
    
        std::vector<std::unique_ptr<CmdNode>> parse(std::vector<Lexer::token>*);

        // Debugging
        std::string parseToString();

        // Instance Variables
        std::vector<Lexer::token>* tokens;
        int token_index = 0;

        // Helpers
        Lexer::TokenType peekToken(int index_to_peek);
        const Lexer::token consumeToken(int index_to_consume, Lexer::TokenType expected_type);

        // Parse Functions
        std::vector<std::unique_ptr<CmdNode>> parseAllTokens();
        CmdNode* parseCmd();
        TypeNode* parseType();
        TypeNode* parseTypeHead();
        TypeNode* parseTypeCont(TypeNode* t);
        ExprNode* parseExpr();
        
        ExprNode* parseBoolopExpr();
        ExprNode* parseBoolopExprCont(std::unique_ptr<ExprNode>& head);

        ExprNode* parseComparisonExpr();
        ExprNode* parseComparisonExprCont(std::unique_ptr<ExprNode>& head);
        
        ExprNode* parseAddExpr();
        ExprNode* parseAddExprCont(std::unique_ptr<ExprNode>& head);
        
        ExprNode* parseMultExpr();
        ExprNode* parseMultExprCont(std::unique_ptr<ExprNode>& head);
        
        ExprNode* parseUnopExpr();
        
        ExprNode* parseBaseExpr();
        ExprNode* parseBaseExprHead();
        ExprNode* parseBaseExprVarCont(const Lexer::token& t);
        ExprNode* parseBaseExprCont(ExprNode* head);
        ArgumentNode* parseArgument();
        LValue* parseLValue();

        BindingNode* parseBinding();
        StmtNode* parseStmt();
        
}

#endif
