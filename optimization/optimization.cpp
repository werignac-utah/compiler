#include "optimization.h"
#include "../trycasts.cpp"

namespace Optimization
{

#pragma region ASTVisitor
    void ASTVisitor::visit_all_cmds(std::vector<std::unique_ptr<Parser::CmdNode>>& cmds)
    {
        for (auto& u_cmd : cmds)
        {
            visit_cmd(u_cmd);
        }
    }

    //cmds
    void ASTVisitor::visit_cmd(std::unique_ptr<Parser::CmdNode>& u_cmd)
    {
        Parser::CmdNode* cmd = u_cmd.get();

        Parser::CmdNode* new_cmd = nullptr;

        {
            Parser::ReadCmdNode* result;
            if (tryCastCmd<Parser::ReadCmdNode>(cmd, result))
            {
                new_cmd = visit_read_cmd(result);
            }
        }

        {
            Parser::WriteCmdNode* result;
            if (tryCastCmd<Parser::WriteCmdNode>(cmd, result))
            {
                new_cmd = visit_write_cmd(result);
            }
        }

        {
            Parser::TypeCmdNode* result;
            if (tryCastCmd<Parser::TypeCmdNode>(cmd, result))
            {
                new_cmd = visit_type_cmd(result);
            }
        }

        {
            Parser::LetCmdNode* result;
            if (tryCastCmd<Parser::LetCmdNode>(cmd, result))
            {
                new_cmd = visit_let_cmd(result);
            }
        }

        {
            Parser::AssertCmdNode* result;
            if (tryCastCmd<Parser::AssertCmdNode>(cmd, result))
            {
                new_cmd = visit_assert_cmd(result);
            }
        }

        {
            Parser::PrintCmdNode* result;
            if (tryCastCmd<Parser::PrintCmdNode>(cmd, result))
            {
                new_cmd = visit_print_cmd(result);
            }
        }

        {
            Parser::ShowCmdNode* result;
            if (tryCastCmd<Parser::ShowCmdNode>(cmd, result))
            {
                new_cmd = visit_show_cmd(result);
            }
        }

        {
            Parser::TimeCmdNode* result;
            if (tryCastCmd<Parser::TimeCmdNode>(cmd, result))
            {
                new_cmd = visit_time_cmd(result);
            }
        }

        {
            Parser::FnCmd* result;
            if (tryCastCmd<Parser::FnCmd>(cmd, result))
            {
                new_cmd = visit_fn_cmd(result);
            }
        }

        if (new_cmd)
            u_cmd.reset(new_cmd);
    }

    Parser::CmdNode* ASTVisitor::visit_read_cmd(Parser::ReadCmdNode* read_cmd)
    {
        return nullptr;
    }

    Parser::CmdNode* ASTVisitor::visit_write_cmd(Parser::WriteCmdNode* write_cmd)
    {
        visit_expr(write_cmd->toSave);
        
        return nullptr;
    }

    Parser::CmdNode* ASTVisitor::visit_type_cmd(Parser::TypeCmdNode* type_cmd)
    {
        return nullptr;
    }

    Parser::CmdNode* ASTVisitor::visit_let_cmd(Parser::LetCmdNode* let_cmd)
    {
        visit_expr(let_cmd->expression);
        return nullptr; 
    }

    Parser::CmdNode* ASTVisitor::visit_assert_cmd(Parser::AssertCmdNode* assert_cmd)
    {
        visit_expr(assert_cmd->expression);
        return nullptr;
    }

    Parser::CmdNode* ASTVisitor::visit_print_cmd(Parser::PrintCmdNode* print_cmd)
    {
        return nullptr;
    }

    Parser::CmdNode* ASTVisitor::visit_show_cmd(Parser::ShowCmdNode* show_cmd)
    {
        visit_expr(show_cmd->expression);
        return nullptr;
    }

    Parser::CmdNode* ASTVisitor::visit_time_cmd(Parser::TimeCmdNode* time_cmd)
    {
        visit_cmd(time_cmd->command);
        return nullptr;
    }

    Parser::CmdNode* ASTVisitor::visit_fn_cmd(Parser::FnCmd* fn_cmd)
    {
        for (auto& u_stmt : fn_cmd->function_contents)
        {
            visit_stmt(u_stmt);
        }
        return nullptr;
    }

    //stmts
    void ASTVisitor::visit_stmt(std::unique_ptr<Parser::StmtNode>& u_stmt)
    {
        Parser::StmtNode* stmt = u_stmt.get();

        Parser::StmtNode* new_stmt = nullptr;

        {
            Parser::LetStmtNode* result;
            if (tryCastStmt<Parser::LetStmtNode>(stmt, result))
            {
                new_stmt = visit_let_stmt(result);
            }
        }

        {
            Parser::AssertStmtNode* result;
            if (tryCastStmt<Parser::AssertStmtNode>(stmt, result))
            {
                new_stmt = visit_assert_stmt(result);
            }
        }

        {
            Parser::ReturnStmtNode* result;
            if (tryCastStmt<Parser::ReturnStmtNode>(stmt, result))
            {
                new_stmt = visit_return_stmt(result);
            }
        }

        if (new_stmt)
            u_stmt.reset(new_stmt);
    }

    Parser::StmtNode* ASTVisitor::visit_let_stmt(Parser::LetStmtNode* let_stmt)
    {
        visit_expr(let_stmt->variable_expression);
        return nullptr;
    }

    Parser::StmtNode* ASTVisitor::visit_assert_stmt(Parser::AssertStmtNode* assert_stmt)
    {
        visit_expr(assert_stmt->expression);
        return nullptr;
    }

    Parser::StmtNode* ASTVisitor::visit_return_stmt(Parser::ReturnStmtNode* return_stmt)
    {
        visit_expr(return_stmt->expression);
        return nullptr;
    }

    //exprs
    void ASTVisitor::visit_expr(std::unique_ptr<Parser::ExprNode>& u_expr)
    {
        Parser::ExprNode* expr = u_expr.get();

        Parser::ExprNode* new_expr = nullptr;

        {
            Parser::IntExprNode* result;
            if (tryCastExpr<Parser::IntExprNode>(expr, result))
            {
                new_expr = visit_int_expr(result);
            }
        }

        {
            Parser::FloatExprNode* result;
            if (tryCastExpr<Parser::FloatExprNode>(expr, result))
            {
                new_expr = visit_float_expr(result);
            }
        }

        {
            Parser::TrueExprNode* result;
            if (tryCastExpr<Parser::TrueExprNode>(expr, result))
            {
                new_expr = visit_true_expr(result);
            }
        }

        {
            Parser::FalseExprNode* result;
            if (tryCastExpr<Parser::FalseExprNode>(expr, result))
            {
                new_expr = visit_false_expr(result);
            }
        }

        {
            Parser::VariableExprNode* result;
            if (tryCastExpr<Parser::VariableExprNode>(expr, result))
            {
                new_expr = visit_variable_expr(result);
            }
        }

        {
            Parser::TupleLiteralExprNode* result;
            if (tryCastExpr<Parser::TupleLiteralExprNode>(expr, result))
            {
                new_expr = visit_tuple_expr(result);
            }
        }

        {
            Parser::ArrayLiteralExprNode* result;
            if (tryCastExpr<Parser::ArrayLiteralExprNode>(expr, result))
            {
                new_expr = visit_array_expr(result);
            }
        }

        {
            Parser::TupleIndexExprNode* result;
            if (tryCastExpr<Parser::TupleIndexExprNode>(expr, result))
            {
                new_expr = visit_tuple_index_expr(result);
            }
        }

        {
            Parser::ArrayIndexExprNode* result;
            if (tryCastExpr<Parser::ArrayIndexExprNode>(expr, result))
            {
                new_expr = visit_array_index_expr(result);
            }
        }

        {
            Parser::CallExprNode* result;
            if (tryCastExpr<Parser::CallExprNode>(expr, result))
            {
                new_expr = visit_call_expr(result);
            }
        }

        {
            Parser::UnopExprNode* result;
            if (tryCastExpr<Parser::UnopExprNode>(expr, result))
            {
                new_expr = visit_unop_expr(result);
            }
        }

        {
            Parser::BinopExprNode* result;
            if (tryCastExpr<Parser::BinopExprNode>(expr, result))
            {
                new_expr = visit_binop_expr(result);
            }
        }

        {
            Parser::IfExprNode* result;
            if (tryCastExpr<Parser::IfExprNode>(expr, result))
            {
                new_expr = visit_if_expr(result);
            }
        }

        {
            Parser::LoopExprNode* result;
            if (tryCastExpr<Parser::LoopExprNode>(expr, result))
            {
                new_expr = visit_loop_expr(result);
            }
        }

        if (new_expr)
            u_expr.reset(new_expr);
    }

    Parser::ExprNode* ASTVisitor::visit_int_expr(Parser::IntExprNode* int_expr)
    {
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_float_expr(Parser::FloatExprNode* float_expr)
    {
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_true_expr(Parser::TrueExprNode* true_expr)
    {
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_false_expr(Parser::FalseExprNode* false_expr)
    {
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_variable_expr(Parser::VariableExprNode* variable_expr)
    {
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_tuple_expr(Parser::TupleLiteralExprNode* tuple_expr)
    {
        for (auto& u_subexpr : tuple_expr->tuple_expressions)
            visit_expr(u_subexpr);
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_array_expr(Parser::ArrayLiteralExprNode* array_expr)
    {
        for (auto& u_subexpr : array_expr->array_expressions)
            visit_expr(u_subexpr);
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_tuple_index_expr(Parser::TupleIndexExprNode* tuple_index_expr)
    {
        visit_expr(tuple_index_expr->tuple_expression);
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_array_index_expr(Parser::ArrayIndexExprNode* array_index_expr)
    {
        visit_expr(array_index_expr->array_expression);
        for (auto& u_subexpr : array_index_expr->array_indices)
            visit_expr(u_subexpr);
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_call_expr(Parser::CallExprNode* call_expr)
    {
        for (auto& u_paramexpr : call_expr->arguments)
            visit_expr(u_paramexpr);
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_unop_expr(Parser::UnopExprNode* unop_expr)
    {
        visit_expr(unop_expr->expression);
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_binop_expr(Parser::BinopExprNode* binop_expr)
    {
        visit_expr(binop_expr->lhs);
        visit_expr(binop_expr->rhs);
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_if_expr(Parser::IfExprNode* if_expr)
    {
        visit_expr(if_expr->condition);
        visit_expr(if_expr->then_expr);
        visit_expr(if_expr->else_expr);
        return nullptr;
    }

    Parser::ExprNode* ASTVisitor::visit_loop_expr(Parser::LoopExprNode* loop_expr)
    {
        for (auto& u_bound : loop_expr->bounds)
            visit_expr(u_bound->second);
        visit_expr(loop_expr->loop_expression);
        return nullptr;
    }

#pragma endregion

#pragma region ConstantPropagation

    ConstantPropagation::ConstantPropagation()
    {
        context["argnum"] = std::make_shared<Parser::CPValue>();
        // May need to change args to use ArrayValue
        std::vector<std::shared_ptr<Parser::CPValue>> lengths = {std::make_shared<Parser::CPValue>()};
        context["args"] = std::make_shared<Parser::ArrayValue>(lengths);
    }

    Parser::ExprNode* ConstantPropagation::visit_int_expr(Parser::IntExprNode* int_expr)
    {
        // May cause mem leak or weird behaviour bc not sure how reset on a shred ptr handles the old value / ref counter.
        int_expr->cp.reset(new Parser::IntValue(int_expr->value));
        return nullptr;
    }

    Parser::CmdNode* ConstantPropagation::visit_let_cmd(Parser::LetCmdNode* let_expr)
    {
        visit_expr(let_expr->expression);

        Parser::ArgumentLValue* arg_lvalue;
        // decompose the lvalue and update context
        if (tryCast<Parser::LValue, Parser::ArgumentLValue>(let_expr->lvalue.get(), arg_lvalue))
        {
            Parser::VarArgumentNode* var_argument;
            if (tryCast<Parser::ArgumentNode, Parser::VarArgumentNode>(arg_lvalue->argument.get(), var_argument))
            {
                context[var_argument->token_s] = let_expr->expression->cp;
            }
            else if (let_expr->expression->cp->type == Parser::CPValue::ARRAY)
            {
                Parser::ArrayValue* array_value = static_cast<Parser::ArrayValue*>(let_expr->expression->cp.get());
                Parser::ArrayArgumentNode* array_argument = static_cast<Parser::ArrayArgumentNode*>(arg_lvalue->argument.get());
                for (int i = 0; i < array_argument->array_dimensions_names.size(); i++)
                {
                    std::string dimension = array_argument->array_dimensions_names[i];
                    std::shared_ptr<Parser::CPValue> dimension_cpv = array_value->lengths[i];
                    context[dimension] = dimension_cpv;
                }
                context[array_argument->array_argument_name] = let_expr->expression->cp;
            }
        }
        return nullptr;
    }

    Parser::StmtNode* ConstantPropagation::visit_let_stmt(Parser::LetStmtNode* let_stmt)
    {
        visit_expr(let_stmt->variable_expression);
        // decompose the lvalue and update context
        if (let_stmt->variable_expression->cp->type == Parser::CPValue::INT)
        {
            Parser::ArgumentLValue* lvalue = static_cast<Parser::ArgumentLValue*>(let_stmt->set_variable_name.get());
            Parser::VarArgumentNode* argument = static_cast<Parser::VarArgumentNode*>(lvalue->argument.get());

            context[argument->token_s] = let_stmt->variable_expression->cp;
        }
        return nullptr;
    }

    Parser::ExprNode* ConstantPropagation::visit_loop_expr(Parser::LoopExprNode* loop_expr)
    {
        Parser::ArrayLoopExprNode* array_loop_expr;
        if (tryCast<Parser::LoopExprNode, Parser::ArrayLoopExprNode>(loop_expr, array_loop_expr))
        {
            std::vector<std::shared_ptr<Parser::CPValue>> array_lengths;
            for (auto& u_bound : loop_expr->bounds)
            {
                visit_expr(u_bound->second);
                array_lengths.push_back(u_bound->second->cp);
            }
            visit_expr(loop_expr->loop_expression);

            std::shared_ptr<Parser::ArrayValue> array_value = std::make_shared<Parser::ArrayValue>(array_lengths);
            loop_expr->cp = array_value;
        }
        else
        {
            ASTVisitor::visit_loop_expr(loop_expr);
        }

        return nullptr;
    }

    Parser::ExprNode* ConstantPropagation::visit_variable_expr(Parser::VariableExprNode* variable_expr)
    {
        if (context[variable_expr->token_s])
            variable_expr->cp = context[variable_expr->token_s];
        return nullptr;
    }







    Parser::CmdNode* ConstantPropagation::visit_read_cmd(Parser::ReadCmdNode* read_cmd)
    {
        std::vector<std::shared_ptr<Parser::CPValue>> lengths = {std::make_shared<Parser::CPValue>(), std::make_shared<Parser::CPValue>()};
        
        Parser::VarArgumentNode* var_argument;
        if (tryCast<Parser::ArgumentNode, Parser::VarArgumentNode>(read_cmd->readInto.get(), var_argument))
            context[var_argument->token_s] = std::make_shared<Parser::ArrayValue>(lengths);
        else
        {
            Parser::ArrayArgumentNode* array_argument = static_cast<Parser::ArrayArgumentNode*>(read_cmd->readInto.get());
            context[array_argument->array_argument_name] = std::make_shared<Parser::ArrayValue>(lengths);
        }
        
        return nullptr;
    }

    Parser::ExprNode* ConstantPropagation::visit_array_expr(Parser::ArrayLiteralExprNode* array_expr)
    {
        for (auto& u_subexpr : array_expr->array_expressions)
            visit_expr(u_subexpr);
        
        std::shared_ptr<Parser::IntValue> one_length = std::make_shared<Parser::IntValue>(array_expr->array_expressions.size());
        std::vector<std::shared_ptr<Parser::CPValue>> array_lengths = {one_length};
        std::shared_ptr<Parser::ArrayValue> array_value = std::make_shared<Parser::ArrayValue>(array_lengths);
        array_expr->cp = array_value; 
        
        return nullptr;
    }
#pragma endregion

}