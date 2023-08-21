#include "../parser/parser.h"
#include "../typechecker/typechecker.h"
#include <vector>
#include <unordered_map>

#ifndef __OPTIMIZATION_H__
#define __OPTIMIZATION_H__

namespace Optimization
{
    class ASTVisitor
    {
    public:
        void visit_all_cmds(std::vector<std::unique_ptr<Parser::CmdNode>>& cmds);
    
    protected:
        virtual ~ASTVisitor() {};
        
        //cmds
        void visit_cmd(std::unique_ptr<Parser::CmdNode>&);
        virtual Parser::CmdNode* visit_read_cmd(Parser::ReadCmdNode*);
        virtual Parser::CmdNode* visit_write_cmd(Parser::WriteCmdNode*);
        virtual Parser::CmdNode* visit_type_cmd(Parser::TypeCmdNode*);
        virtual Parser::CmdNode* visit_let_cmd(Parser::LetCmdNode*);
        virtual Parser::CmdNode* visit_assert_cmd(Parser::AssertCmdNode*);
        virtual Parser::CmdNode* visit_print_cmd(Parser::PrintCmdNode*);
        virtual Parser::CmdNode* visit_show_cmd(Parser::ShowCmdNode*);
        virtual Parser::CmdNode* visit_time_cmd(Parser::TimeCmdNode*);
        virtual Parser::CmdNode* visit_fn_cmd(Parser::FnCmd*);

        //stmts
        void visit_stmt(std::unique_ptr<Parser::StmtNode>&);
        virtual Parser::StmtNode* visit_let_stmt(Parser::LetStmtNode*);
        virtual Parser::StmtNode* visit_assert_stmt(Parser::AssertStmtNode*);
        virtual Parser::StmtNode* visit_return_stmt(Parser::ReturnStmtNode*);
        
        //exprs
        void visit_expr(std::unique_ptr<Parser::ExprNode>&);
        virtual Parser::ExprNode* visit_int_expr(Parser::IntExprNode*);
        virtual Parser::ExprNode* visit_float_expr(Parser::FloatExprNode*);
        virtual Parser::ExprNode* visit_true_expr(Parser::TrueExprNode*);
        virtual Parser::ExprNode* visit_false_expr(Parser::FalseExprNode*);
        virtual Parser::ExprNode* visit_variable_expr(Parser::VariableExprNode*);
        virtual Parser::ExprNode* visit_tuple_expr(Parser::TupleLiteralExprNode*);
        virtual Parser::ExprNode* visit_array_expr(Parser::ArrayLiteralExprNode*);
        virtual Parser::ExprNode* visit_tuple_index_expr(Parser::TupleIndexExprNode*);
        virtual Parser::ExprNode* visit_array_index_expr(Parser::ArrayIndexExprNode*);
        virtual Parser::ExprNode* visit_call_expr(Parser::CallExprNode*);
        virtual Parser::ExprNode* visit_unop_expr(Parser::UnopExprNode*);
        virtual Parser::ExprNode* visit_binop_expr(Parser::BinopExprNode*);
        virtual Parser::ExprNode* visit_if_expr(Parser::IfExprNode*);
        virtual Parser::ExprNode* visit_loop_expr(Parser::LoopExprNode*);
    };

    class ConstantPropagation : public ASTVisitor
    {
    private:
        std::unordered_map<std::string, std::shared_ptr<Parser::CPValue>> context;

    public:
        virtual ~ConstantPropagation() {};
        ConstantPropagation();

    protected:
        virtual Parser::ExprNode* visit_int_expr(Parser::IntExprNode*) override;
        virtual Parser::CmdNode* visit_let_cmd(Parser::LetCmdNode*) override;
        virtual Parser::StmtNode* visit_let_stmt(Parser::LetStmtNode*) override;
        virtual Parser::ExprNode* visit_loop_expr(Parser::LoopExprNode*) override;
        virtual Parser::ExprNode* visit_variable_expr(Parser::VariableExprNode*) override;

        virtual Parser::CmdNode* visit_read_cmd(Parser::ReadCmdNode*) override;
        virtual Parser::ExprNode* visit_array_expr(Parser::ArrayLiteralExprNode*) override;
    };
}

#endif