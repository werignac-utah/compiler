#include "typechecker.h"
#include "../trycasts.cpp"
#include <type_traits>

namespace Typechecker
{
    TypeException::TypeException(const std::string& m, const Parser::ASTNode* node)
    {
        message = "\nEncountered error at Typechecking Step. Line ";
        message += std::to_string(node->line) + ", Position ";
        message += std::to_string(node->pos) + ", Expression ";
        message += node->token_s + ".\n" + m;
    }

    const char* TypeException::what () const noexcept
    {
        return message.c_str();
    }

#pragma region Scope

    std::shared_ptr<Scope> Scope::createNestedScope()
    {
        std::shared_ptr<Scope> child = std::make_shared<Scope>();
        child->parent = this;
        return child;
    }

    bool Scope::add(std::string name, NameInfo* info)
    {
        [[maybe_unused]] NameInfo* _;
        if (lookup(name, _))
            return false;
        
        symbol_table.insert(std::make_pair(name, std::unique_ptr<NameInfo>(info)));
        return true;
    }

    void Scope::add_argument(Parser::ArgumentNode* argument, std::shared_ptr<ResolvedType>& rtype)
    {
        // <variable>
        {
            Parser::VarArgumentNode* result;
            if (tryCast<Parser::ArgumentNode, Parser::VarArgumentNode>(argument, result))
            {
                NameInfo* varinfo = new VariableInfo(rtype);
                if (!add(result->token_s, varinfo))
                {
                    delete varinfo;
                    throw TypeException("Caught argument with already defined name \"" + result->token_s + "\".", argument);
                }
                return;
            }
        }

        // <variable> [ <variable> , ... ]
        {
            Parser::ArrayArgumentNode* result;
            if (tryCast<Parser::ArgumentNode, Parser::ArrayArgumentNode>(argument, result))
            {
                ArrayRType* array_rtype;
                if (!tryCast<ResolvedType, ArrayRType>(rtype.get(), array_rtype))
                    throw TypeException("Caught an array argument assigned non-array type. Got a type of " + rtype->toString() + ".", argument);
                if (result->array_dimensions_names.size() != array_rtype->rank)
                    throw TypeException("Caught an argument array rank mis-match. The argument expected an array of rank " + std::to_string(result->array_dimensions_names.size()) + " but was assigned an array of size " + std::to_string(array_rtype->rank) + ".", argument);
                NameInfo* varinfo = new VariableInfo(rtype);
                if (! add(result->array_argument_name, varinfo))
                {
                    delete varinfo;
                    throw TypeException("Caught argument with already defined name \"" + result->array_argument_name + "\".", argument);
                }
                for (std::string& dimension_name : result->array_dimensions_names)
                {
                    std::shared_ptr<ResolvedType> intType = std::make_shared<IntRType>();
                    NameInfo* dim_varinfo = new VariableInfo(intType);
                    if (! add(dimension_name, dim_varinfo))
                    {
                        delete dim_varinfo;
                        throw TypeException("Caught argument dimension with already defined name \"" + dimension_name + "\".", argument);
                    }
                }
                return;
            }
        }
    }

    void Scope::add_lvalue(Parser::LValue* lvalue, std::shared_ptr<ResolvedType>& rtype)
    {
        // <argument>
        {
            Parser::ArgumentLValue* result;
            if (tryCast<Parser::LValue, Parser::ArgumentLValue>(lvalue, result))
            {
                add_argument(result->argument.get(), rtype);
                return;
            }
        }

        // pseudo arg
        {
            PseudoArgumentLValue* result;
            if (tryCast<Parser::LValue, PseudoArgumentLValue>(lvalue, result))
            {
                add_argument(result->argument, rtype);
                return;
            }
        }

        // { <lvalue>, }
        {
            Parser::TupleLValueNode* result;
            if (tryCast<Parser::LValue, Parser::TupleLValueNode>(lvalue, result))
            {
                TupleRType* tuple_rtype;
                if (! tryCast<ResolvedType, TupleRType>(rtype.get(), tuple_rtype))
                    throw TypeException("Caught tuple lvalue assigned non-tuple type: " + rtype->toString() + ".", lvalue);
                if (result->lvalues.size() != tuple_rtype->element_types.size())
                    throw TypeException("Caught tuple lvalue assinged a tuple type with a different number of elements. LValue: " + std::to_string(result->lvalues.size()) + ", Assigned Type: " + std::to_string(tuple_rtype->element_types.size()) + ".", lvalue);
                for (int i = 0; i < result->lvalues.size(); i++)
                {
                    Parser::LValue* sub_lvalue = result->lvalues[i].get();
                    std::shared_ptr<ResolvedType> sub_rtype = tuple_rtype->element_types[i];
                    add_lvalue(sub_lvalue, sub_rtype);
                }
                return;
            }
        }

        // pseudo tuple
        {
            PseudoTupleLValue* result;
            if (tryCast<Parser::LValue, PseudoTupleLValue>(lvalue, result))
            {
                TupleRType* tuple_rtype;
                if (! tryCast<ResolvedType, TupleRType>(rtype.get(), tuple_rtype))
                    throw TypeException("Caught tuple lvalue assigned non-tuple type: " + rtype->toString() + ".", lvalue);
                if (result->lvalues.size() != tuple_rtype->element_types.size())
                    throw TypeException("Caught tuple lvalue assinged a tuple type with a different number of elements. LValue: " + std::to_string(result->lvalues.size()) + ", Assigned Type: " + std::to_string(tuple_rtype->element_types.size()) + ".", lvalue);
                for (int i = 0; i < result->lvalues.size(); i++)
                {
                    Parser::LValue* sub_lvalue = result->lvalues[i].get();
                    std::shared_ptr<ResolvedType> sub_rtype = tuple_rtype->element_types[i];
                    add_lvalue(sub_lvalue, sub_rtype);
                }
                return;
            }
        }
    }

    bool Scope::lookup(std::string name, NameInfo*& info)
    {     
        if (symbol_table.count(name) > 0)
        {
            info = symbol_table[name].get();
            return true;
        }
        else if (parent != nullptr)
            return parent->lookup(name, info);
        else
            return false;
    }

    NameInfo* Scope::get_info(std::string name)
    {
        return symbol_table[name].get();
    }

    std::shared_ptr<Scope> create_global_scope()
    {
        std::shared_ptr<Scope> global_scope = std::make_shared<Scope>();

        // args (int[])
        {
            std::shared_ptr<ResolvedType> arg_type = std::make_shared<IntRType>();
            std::shared_ptr<ResolvedType> arg_types = ArrayRType::make_array(arg_type, 1);
            global_scope->add("args",new VariableInfo(arg_types));
        }

        // argnum (int)
        {
            std::shared_ptr<ResolvedType> argnum_type = std::make_shared<IntRType>();
            global_scope->add("argnum", new VariableInfo(argnum_type));
        }
        
        // runtime functions
        {
            std::shared_ptr<ResolvedType> return_type = std::make_shared<FloatRType>();
            std::vector<std::shared_ptr<ResolvedType>> args = {std::make_shared<FloatRType>()};
            // float sqrt(float)
            global_scope->add("sqrt", new FuncInfo(return_type, args));
            // float exp(float)
            global_scope->add("exp", new FuncInfo(return_type, args));
            // float sin(float)
            global_scope->add("sin", new FuncInfo(return_type, args));
            // float cos(float)
            global_scope->add("cos", new FuncInfo(return_type, args));
            // float tan(float)
            global_scope->add("tan", new FuncInfo(return_type, args));
            // float asin(float)
            global_scope->add("asin", new FuncInfo(return_type, args));
            // float acos(float)
            global_scope->add("acos", new FuncInfo(return_type, args));
            // float atan(float)
            global_scope->add("atan", new FuncInfo(return_type, args));
            // float log(float)
            global_scope->add("log", new FuncInfo(return_type, args));
        }
        {
            std::shared_ptr<ResolvedType> return_type = std::make_shared<FloatRType>();
            std::vector<std::shared_ptr<ResolvedType>> args = {std::make_shared<FloatRType>(), std::make_shared<FloatRType>()};
            // float pow(float, float)
            global_scope->add("pow", new FuncInfo(return_type, args));
            // float atan2(float, float)
            global_scope->add("atan2", new FuncInfo(return_type, args));
        }
        // int to_int(float)
        {
            std::shared_ptr<ResolvedType> return_type = std::make_shared<IntRType>();
            std::vector<std::shared_ptr<ResolvedType>> args = {std::make_shared<FloatRType>()};
            global_scope->add("to_int", new FuncInfo(return_type, args));
        }
        // float to_float(int)
        {
            std::shared_ptr<ResolvedType> return_type = std::make_shared<FloatRType>();
            std::vector<std::shared_ptr<ResolvedType>> args = {std::make_shared<IntRType>()};
            global_scope->add("to_float", new FuncInfo(return_type, args));
        }

        return global_scope;
    }

#pragma endregion

    std::shared_ptr<ResolvedType> resolve_type(std::unique_ptr<Parser::TypeNode>& u_type, std::shared_ptr<Scope>& scope)
    {
        Parser::TypeNode* _type = u_type.get();

        // int
        {
            Parser::IntTypeNode* result;
            if (tryCast<Parser::TypeNode, Parser::IntTypeNode>(_type, result))
                return std::make_shared<IntRType>();
        }
        // float
        {
            Parser::FloatTypeNode* result;
            if (tryCast<Parser::TypeNode, Parser::FloatTypeNode>(_type, result))
                return std::make_shared<FloatRType>();
        }
        // bool
        {
            Parser::BoolTypeNode* result;
            if (tryCast<Parser::TypeNode, Parser::BoolTypeNode>(_type, result))
                return std::make_shared<BoolRType>();
        }
        // <variable>
        {
            Parser::VariableTypeNode* result;
            if (tryCast<Parser::TypeNode, Parser::VariableTypeNode>(_type, result))
            {
                NameInfo* info;
                if (! scope->lookup(result->token_s, info))
                    throw TypeException("Undefined reference to type variable " + result->token_s + ".", _type);
                TypeInfo* typeinfo;
                if (! tryCast<NameInfo, TypeInfo>(info, typeinfo))
                    throw TypeException("Reference to variable " + result->token_s + " as a type value; but it isn't.", _type);
                return typeinfo->stored_type;
            }
        }
        // <type> [, ... ]
        {
            Parser::ArrayTypeNode* result;
            if (tryCast<Parser::TypeNode, Parser::ArrayTypeNode>(_type, result))
            {
                std::shared_ptr<ResolvedType> element_type = resolve_type(result->array_type, scope);
                return std::make_shared<ArrayRType>(element_type, result->rank);
            }
        }
        // {<type>, ... }
        {
            Parser::TupleTypeNode* result;
            if (tryCast<Parser::TypeNode, Parser::TupleTypeNode>(_type, result))
            {
                std::vector<std::shared_ptr<ResolvedType>> r_types;
                for (std::unique_ptr<Parser::TypeNode>& tuple_element_type : result->tuple_types)
                    r_types.push_back(resolve_type(tuple_element_type, scope));
                return std::make_shared<TupleRType>(r_types);
            }
        }

        throw TypeException("Could not identify type.", _type);
    }

    std::shared_ptr<Scope> type_of_loop_bounds(std::vector<std::unique_ptr<std::pair<std::string, std::unique_ptr<Parser::ExprNode>>>>& bounds, std::shared_ptr<Scope>& scope)
    {
        std::shared_ptr<Scope> child_scope = scope->createNestedScope();

        for(auto& index_pair : bounds)
        {
            std::unique_ptr<Parser::ExprNode>& sub_expr = index_pair->second;
            std::shared_ptr<ResolvedType> bound_rtype = type_of(sub_expr, scope);
            IntRType* bound_rtype_as_int;
            if (! tryCast<ResolvedType, IntRType>(bound_rtype.get(), bound_rtype_as_int))
                throw TypeException("Caught loop iterating over non-int type: " + bound_rtype->toString() + ".", sub_expr.get());
            sub_expr->resolvedType = bound_rtype;
            NameInfo* info = new VariableInfo(bound_rtype);
            if (! child_scope->add(index_pair->first, info))
            {
                delete info;
                throw TypeException("Caught loop iterating variable with already defined name \"" + index_pair->first + "\".", sub_expr.get());
            }
        }

        return child_scope;
    }

    std::shared_ptr<ResolvedType> type_of(std::unique_ptr<Parser::ExprNode>& u_expr, std::shared_ptr<Scope>& scope)
    {
        Parser::ExprNode* expr = u_expr.get();

        // <integer>
        {
            Parser::IntExprNode* result;
            if (tryCastExpr<Parser::IntExprNode>(expr, result))
                return std::make_shared<IntRType>();
        }
        // <float>
        {
            Parser::FloatExprNode* result;
            if (tryCastExpr<Parser::FloatExprNode>(expr, result))
                return std::make_shared<FloatRType>();
        }
        // true
        {
            Parser::TrueExprNode* result;
            if (tryCastExpr<Parser::TrueExprNode>(expr, result))
                return std::make_shared<BoolRType>();
        }
        // false
        {
            Parser::FalseExprNode* result;
            if (tryCastExpr<Parser::FalseExprNode>(expr, result))
                return std::make_shared<BoolRType>();
        }

        // binop exprs
        {
            Parser::BinopExprNode* result;
            if (tryCastExpr<Parser::BinopExprNode>(expr, result))
            {
                std::shared_ptr<ResolvedType> lhs_rtype = type_of(result->lhs, scope);
                std::shared_ptr<ResolvedType> rhs_rtype = type_of(result->rhs, scope);

                result->lhs.get()->resolvedType = lhs_rtype;
                result->rhs.get()->resolvedType = rhs_rtype;
                
                switch (result->operation)
                {
                // <expr> + <expr>
                case Parser::BinopExprNode::PLUS:
                // <expr> - <expr>
                case Parser::BinopExprNode::MINUS:
                // <expr> * <expr>
                case Parser::BinopExprNode::TIMES:
                // <expr> / <expr>
                case Parser::BinopExprNode::DIVIDE:
                // <expr> % <expr>
                case Parser::BinopExprNode::MOD:
                    {
                        if (*lhs_rtype.get() != *rhs_rtype.get())
                            throw TypeException("Types do not match for arithmetic operation. lhs: " + lhs_rtype.get()->toString() + " rhs: " + rhs_rtype.get()->toString(), result);
                        
                        if (lhs_rtype.get()->type_name == INT)
                            return std::make_shared<IntRType>();
                        
                        if (lhs_rtype.get()->type_name == FLOAT)
                            return std::make_shared<FloatRType>();
                        
                        throw TypeException("No supported arithmetic operation for " + lhs_rtype.get()->toString() + ". Expects two ints or floats.", result);
                    }
                // <expr> < <expr>
                case Parser::BinopExprNode::LESS_THAN:
                // <expr> > <expr>
                case Parser::BinopExprNode::GREATER_THAN:
                // <expr> <= <expr>
                case Parser::BinopExprNode::GREATER_THAN_OR_EQUALS:
                // <expr> >= <expr>
                case Parser::BinopExprNode::LESS_THAN_OR_EQUALS:
                    {
                        if (*lhs_rtype.get() != *rhs_rtype.get())
                            throw TypeException("Types do not match for comparison operation. lhs: " + lhs_rtype.get()->toString() + " rhs: " + rhs_rtype.get()->toString(), result);
                        
                        bool int_args = lhs_rtype.get()->type_name == INT;
                        bool float_args = lhs_rtype.get()->type_name == FLOAT;

                        if (!(int_args || float_args))
                            throw TypeException("No supported comparison operation for " + lhs_rtype.get()->toString() + ". Expects two ints or floats.", result);
                        
                        return std::make_shared<BoolRType>();
                    }
                // <expr> == <expr>
                case Parser::BinopExprNode::EQUALS:
                // <expr> != <expr>
                case Parser::BinopExprNode::NOT_EQUALS:
                    {
                        if (*lhs_rtype.get() != *rhs_rtype.get())
                            throw TypeException("Types do not match for equality operation. lhs: " + lhs_rtype.get()->toString() + " rhs: " + rhs_rtype.get()->toString(), result);
                        
                        bool int_args = lhs_rtype.get()->type_name == INT;
                        bool float_args = lhs_rtype.get()->type_name == FLOAT;
                        bool bool_args = lhs_rtype.get()->type_name == BOOL;

                        if (!(int_args || float_args || bool_args))
                            throw TypeException("No supported equality operation for " + lhs_rtype.get()->toString() + ". Expects two ints, floats, or bools.", result);
                        
                        return std::make_shared<BoolRType>();
                    }
                // <expr> && <expr>
                case Parser::BinopExprNode::AND:
                // <expr> || <expr>
                case Parser::BinopExprNode::OR:
                    {
                        if (lhs_rtype.get()->type_name != BOOL || rhs_rtype.get()->type_name != BOOL)
                            throw TypeException("No supported boolean operation for given types. Expects two booleans. lhs: " + lhs_rtype.get()->toString() + " rhs: " + rhs_rtype.get()->toString(), result);
                        
                        return std::make_shared<BoolRType>();
                    }
                }
            }
        }

        // unop exprs
        {
            Parser::UnopExprNode* result;
            if (tryCastExpr<Parser::UnopExprNode>(expr, result))
            {
                std::unique_ptr<Parser::ExprNode>& u_expr = result->expression;
                std::shared_ptr<ResolvedType> expr_rtype = type_of(u_expr, scope);
                u_expr.get()->resolvedType = expr_rtype;

                switch (result->operation)
                {
                // - <expr>
                case Parser::UnopExprNode::NEGATION:
                    {
                        if (expr_rtype.get()->type_name == INT)
                            return std::make_shared<IntRType>();
                        if (expr_rtype.get()->type_name == FLOAT)
                            return std::make_shared<FloatRType>();
                        throw TypeException("No supported unary - for " + expr_rtype.get()->toString() + ". Expects an int or float.", result);
                    }
                // ! <expr>
                case Parser::UnopExprNode::NOT:
                    {
                        if (expr_rtype.get()->type_name != BOOL)
                            throw TypeException("No supported unary ! for " + expr_rtype.get()->toString() + ". Expects a boolean.", result);
                        return std::make_shared<BoolRType>();
                    }
                }
                
            }
        }

        // { <expr>, ... }
        {
            Parser::TupleLiteralExprNode* result;
            if (tryCastExpr<Parser::TupleLiteralExprNode>(expr, result))
            {
                std::vector<std::shared_ptr<ResolvedType>> expression_rtypes;
                for(std::unique_ptr<Parser::ExprNode>& sub_expr : result->tuple_expressions)
                {
                    std::shared_ptr<ResolvedType> sub_expr_rtype = type_of(sub_expr, scope);
                    expression_rtypes.push_back(sub_expr_rtype);
                    sub_expr.get()->resolvedType = sub_expr_rtype;
                }

                return std::make_shared<TupleRType>(expression_rtypes);
            }
        }
        // [ <expr>, ... ]
        {
            Parser::ArrayLiteralExprNode* result;
            if (tryCastExpr<Parser::ArrayLiteralExprNode>(expr, result))
            {
                int element_count = result->array_expressions.size();

                if (element_count == 0)
                    throw TypeException("Caught array literal expression with no elements. Unidentifyable subtype.", result);
                
                std::shared_ptr<ResolvedType> element_rtype = type_of(result->array_expressions[0], scope);
                result->array_expressions[0].get()->resolvedType = element_rtype;

                for (int i = 1; i < element_count; i++)
                {
                    std::shared_ptr<ResolvedType> _elmnt_rtype = type_of(result->array_expressions[i], scope);
                    result->array_expressions[i].get()->resolvedType = _elmnt_rtype;
                    if (*element_rtype.get() != *_elmnt_rtype.get())
                        throw TypeException("Caught array literal with mismatched element types. 1st type: " + element_rtype.get()->toString() + ", " + std::to_string(i + 1) + "th type: " + _elmnt_rtype.get()->toString(), result);
                }

                return std::make_shared<ArrayRType>(element_rtype, 1);
            }
        }
        // if <expr> then <expr> else <expr>
        {
            Parser::IfExprNode* result;
            if (tryCastExpr<Parser::IfExprNode>(expr, result))
            {
                std::shared_ptr<ResolvedType> condition_rtype = type_of(result->condition, scope);
                result->condition.get()->resolvedType = condition_rtype;
                std::shared_ptr<ResolvedType> then_rtype = type_of(result->then_expr, scope);
                result->then_expr.get()->resolvedType = then_rtype;
                std::shared_ptr<ResolvedType> else_rtype = type_of(result->else_expr, scope);
                result->else_expr.get()->resolvedType = else_rtype;

                if (condition_rtype.get()->type_name != BOOL)
                    throw TypeException("Caught if expression with non-boolean conditional expression type " + condition_rtype.get()->toString() + ".", result);
                
                if (*then_rtype.get() != *else_rtype.get())
                    throw TypeException("Caught if expression with non-matching then else expressions. Then: " + then_rtype.get()->toString() + "Else: " + else_rtype.get()->toString() + ".", result);
                
                return std::shared_ptr<ResolvedType>(then_rtype.get()->clone());
            }
        }
        // <expr> { <integer> }
        {
            Parser::TupleIndexExprNode* result;
            if (tryCastExpr<Parser::TupleIndexExprNode>(expr, result))
            {
                std::shared_ptr<ResolvedType> expr_rtype = type_of(result->tuple_expression, scope);
                result->tuple_expression.get()->resolvedType = expr_rtype;

                if (expr_rtype.get()->type_name != TUPLE)
                    throw TypeException("Caught tuple indexing into a non-tuple expression. Expression type: " + expr_rtype.get()->toString(), result);
                
                const TupleRType* tuple_expr_rtype = static_cast<TupleRType*>(expr_rtype.get());

                if (tuple_expr_rtype->element_types.size() <= result->tuple_index || result->tuple_index < 0)
                    throw TypeException("Caught indexing into a tuple with " + std::to_string(tuple_expr_rtype->element_types.size()) + " elements at illegal index " + std::to_string(result->tuple_index), result);

                return std::shared_ptr<ResolvedType>(tuple_expr_rtype->element_types[result->tuple_index].get()->clone()); 
            }
        }
        // <expr> [ <expr>, ... ]
        {
            Parser::ArrayIndexExprNode* result;
            if (tryCastExpr<Parser::ArrayIndexExprNode>(expr, result))
            {
                std::shared_ptr<ResolvedType> expr_rtype = type_of(result->array_expression, scope);
                result->array_expression.get()->resolvedType = expr_rtype;

                if (expr_rtype.get()->type_name != ARRAY)
                    throw TypeException("Caught array indexing into a non-array expression. Expression type: " + expr_rtype.get()->toString(), result);
                
                const ArrayRType* array_expr_rtype = static_cast<ArrayRType*>(expr_rtype.get());

                if (array_expr_rtype->rank != result->array_indices.size())
                    throw TypeException("Caught indexing into an array with rank " + std::to_string(array_expr_rtype->rank) + " with " + std::to_string(result->array_indices.size()) + " indices.", result);
                
                for (std::unique_ptr<Parser::ExprNode>& index_expr : result->array_indices)
                {
                    std::shared_ptr<ResolvedType> index_expr_type = type_of(index_expr, scope);
                    index_expr.get()->resolvedType = index_expr_type;
                    if (index_expr_type.get()->type_name != INT)
                        throw TypeException("Caught indexing into an array with non-int index expression. Expression type: " + index_expr_type.get()->toString(), result);
                }

                return std::shared_ptr<ResolvedType>(array_expr_rtype->element_type.get()->clone());
            }
        }
        // <variable>
        {
            Parser::VariableExprNode* result;
            if (tryCastExpr<Parser::VariableExprNode>(expr, result))
            {
                NameInfo* info;
                if (! scope->lookup(result->token_s, info))
                    throw TypeException("Undefined reference to variable " + result->token_s + ".", expr);
                VariableInfo* varinfo;
                if (! tryCast<NameInfo, VariableInfo>(info, varinfo))
                    throw TypeException("Reference to variable " + result->token_s + " as an expression value; but it isn't.", expr);
                return varinfo->rtype;
            }
        }
        // array [ <variable> : <expr> , ... ] <expr>
        {
            Parser::ArrayLoopExprNode* result;
            if (tryCastExpr<Parser::ArrayLoopExprNode>(expr, result))
            {
                // Add all index iterators to the scope.
                 if (result->bounds.size() < 1)
                    throw TypeException("Caught array loop with no bounds.", expr);
                std::shared_ptr<Scope> child_scope = type_of_loop_bounds(result->bounds, scope);
                // Get the expression type.
                std::shared_ptr<ResolvedType> array_rtype = type_of(result->loop_expression, child_scope);
                result->loop_expression->resolvedType = array_rtype;
                // Turn it into an array and return.
                return ArrayRType::make_array(array_rtype, result->bounds.size());
            }
        }
        // sum [ <variable> : <expr> , ... ] <expr>
        {
            Parser::SumLoopExprNode* result;
            if (tryCastExpr<Parser::SumLoopExprNode>(expr, result))
            {
                // Add all index iterators to the scope.
                if (result->bounds.size() < 1)
                    throw TypeException("Caught sum loop with no bounds.", expr);
                std::shared_ptr<Scope> child_scope = type_of_loop_bounds(result->bounds, scope);
                // Check that the expression is an int or float.
                std::shared_ptr<ResolvedType> sum_rtype = type_of(result->loop_expression, child_scope);
                result->loop_expression->resolvedType = sum_rtype;
                if (sum_rtype->type_name == INT)
                    return std::make_shared<IntRType>();
                if (sum_rtype->type_name == FLOAT)
                    return std::make_shared<FloatRType>();
                throw TypeException("Caught sum loop with non-numerical type " + sum_rtype->toString() + ". Expected an int or a float.", expr);
            }
        }

        // <variable> ( <expr> , ... )
        {
            Parser::CallExprNode* result;
            if (tryCastExpr<Parser::CallExprNode>(expr, result))
            {
                NameInfo* info;
                if (! scope->lookup(result->function_name, info))
                    throw TypeException("Undefined reference to function " + result->function_name + ".", expr);
                FuncInfo* funcinfo;
                if (! tryCast<NameInfo, FuncInfo>(info, funcinfo))
                    throw TypeException("Referenced non-function " + result->function_name + " as a function.", expr);
                if (funcinfo->arguments.size() != result->arguments.size())
                    throw TypeException("Function " + result->function_name + " expects " + std::to_string(funcinfo->arguments.size()) + " arguments, but got " + std::to_string(result->arguments.size()) + ".", result);
                for (int i = 0; i < funcinfo->arguments.size(); i++)
                {
                    std::shared_ptr<ResolvedType> expected_type = funcinfo->arguments[i];
                    std::shared_ptr<ResolvedType> got_type = type_of(result->arguments[i], scope);
                    if (*expected_type != *got_type)
                        throw TypeException("Function " + result->function_name + " expects a " + expected_type->toString() + " as its " + std::to_string(i + 1) + "th argument, but got a " + got_type->toString() + ".", result);
                    result->arguments[i]->resolvedType = got_type;
                }
                return funcinfo->return_type;
            }
        }

        throw TypeException("Could not identify expression type.", expr);
    }

    std::shared_ptr<Scope> typecheck(std::vector<std::unique_ptr<Parser::CmdNode>>& commands)
    {
        std::shared_ptr<Scope> scope = create_global_scope();

        for (std::unique_ptr<Parser::CmdNode>& u_cmd : commands)
        {
            typecheckCmd(u_cmd, scope);
        }

        return scope;
    }

    void typecheckCmd(std::unique_ptr<Parser::CmdNode>& u_cmd, std::shared_ptr<Scope>& scope)
    {
        Parser::CmdNode* cmd = u_cmd.get();
            
        // show <expr>
        {
            Parser::ShowCmdNode* result;
            if (tryCastCmd<Parser::ShowCmdNode>(cmd, result))
            {
                std::shared_ptr<ResolvedType> rtype = type_of(result->expression, scope);
                result->expression->resolvedType = rtype;
                return;
            }
        }
        
        // read image <string> to <argument>
        {
            Parser::ReadCmdNode* result;
            if (tryCastCmd<Parser::ReadCmdNode>(cmd, result))
            {
                // TODO: Add image to scope.
                std::vector<std::shared_ptr<ResolvedType>> tuple_floats;
                for (int i = 0; i < 4; i++)
                    tuple_floats.push_back(std::make_shared<FloatRType>());
                
                std::shared_ptr<ResolvedType> tuple = std::make_shared<TupleRType>(tuple_floats);
                std::shared_ptr<ResolvedType> array = std::make_shared<ArrayRType>(tuple, 2);

                Parser::ArgumentNode* argument = result->readInto.get();
                scope->add_argument(argument, array);
                return;
            }
        }
        
        // write image <expr> to <string>
        {
            Parser::WriteCmdNode* result;
            if (tryCastCmd<Parser::WriteCmdNode>(cmd, result))
            {
                ArrayRType* target_type;
                std::shared_ptr<ResolvedType> rtype = type_of(result->toSave, scope);
                result->toSave->resolvedType = rtype;
                if (! tryCast<ResolvedType, ArrayRType>(rtype.get(), target_type))
                    throw TypeException("Caught write with expression of non-array type " + rtype->toString() + ". Write expects a {float, float, float, float}[,].", result);
                TupleRType* target_element_type;
                // Check for tuple elements
                if (! tryCast<ResolvedType, TupleRType>(target_type->element_type.get(), target_element_type))
                    throw TypeException("Caught write with expression of non-tuple array type " + rtype->toString() + ". Write expects a {float, float, float, float}[,].", result);
                // Check for rgba pixel values
                if (target_element_type->element_types.size() != 4)
                    throw TypeException("Caught write with expression of non-4-float tuple array type " + rtype->toString() + ". Write expects a {float, float, float, float}[,].", result);
                // Check for rgba float pixel values
                for (const std::shared_ptr<ResolvedType>& tuple_subtype : target_element_type->element_types)
                {
                    if (tuple_subtype->type_name != FLOAT)
                        throw TypeException("Caught write with expression of non-4-float tuple array type " + rtype->toString() + ". Write expects a {float, float, float, float}[,].", result);
                }
                // Check for 2D
                if (target_type->rank != 2)
                    throw TypeException("Caught write with expression of non-rank-2 4-float tuple array type " + rtype->toString() + ". Write expects a {float, float, float, float}[,].", result);
                return;
            }
        }
        
        // let <lvalue> = <expr>
        {
            Parser::LetCmdNode* result;
            if (tryCastCmd<Parser::LetCmdNode>(cmd, result))
            {
                std::shared_ptr<ResolvedType> rtype = type_of(result->expression, scope);
                result->expression->resolvedType = rtype;
                scope->add_lvalue(result->lvalue.get(), rtype);
                return;
            }
        }
        
        // assert <expr> , <string>
        {
            Parser::AssertCmdNode* result;
            if (tryCastCmd<Parser::AssertCmdNode>(cmd, result))
            {
                std::shared_ptr<ResolvedType> rtype = type_of(result->expression, scope);
                if (rtype.get()->type_name != BOOL)
                    throw TypeException("Assert takes a boolean as its first argument. Detected an assert with an expression of type " + rtype->toString() + ".", result);
                result->expression->resolvedType = rtype;
                return;
            }
        }
        
        // print <string>
        {
            Parser::PrintCmdNode* result;
            if (tryCastCmd<Parser::PrintCmdNode>(cmd, result))
                return;
        }
        
        // time <cmd>
        {
            Parser::TimeCmdNode* result;
            if (tryCastCmd<Parser::TimeCmdNode>(cmd, result))
            {
                typecheckCmd(result->command, scope);
                return;
            }
        }

        // type <variable> = <type>
        {
            Parser::TypeCmdNode* result;
            if (tryCastCmd<Parser::TypeCmdNode>(cmd, result))
            {
                TypeInfo* typeinfo = new TypeInfo(resolve_type(result->type, scope));
                
                if (! scope->add(result->variable, typeinfo))
                {
                    delete typeinfo;
                    throw TypeException("Defined variable " + result->variable + " twice.", cmd);
                }
                return;
            }
        }

        // fn <variable> ( <binding> , ... ) : <type> { ;
        //   <stmt> ; ... ;
        // }
        {
            Parser::FnCmd* result;
            if (tryCastCmd<Parser::FnCmd> (cmd, result))
            {
                std::shared_ptr<Scope> function_scope = scope->createNestedScope();
                result->scope = function_scope;


                std::vector<std::unique_ptr<PseudoLValue>> arg_lvalues;
                std::vector<std::shared_ptr<ResolvedType>> arg_rtypes;

                for (int i = 0; i < result->arguments.size(); i++)
                {
                    std::unique_ptr<Parser::BindingNode>& u_binding = result->arguments[i]; 
                    std::pair<PseudoLValue*, std::shared_ptr<ResolvedType>> p = decompose_binding(u_binding, scope);
                    arg_lvalues.emplace_back(p.first);
                    arg_rtypes.push_back(p.second);

                    function_scope->add_lvalue(p.first, p.second);
                }

                std::shared_ptr<ResolvedType> return_type = resolve_type(result->return_type, scope);

                FuncInfo* funcinfo = new FuncInfo(return_type, arg_rtypes);
                
                if (! scope->add(result->function_name, funcinfo))
                {
                    delete funcinfo;
                    throw TypeException("Function " + result->function_name + " was defined twice.", cmd);
                }

                bool has_return = false;

                for(std::unique_ptr<Parser::StmtNode>& u_stmt : result->function_contents)
                {
                    has_return = has_return | typecheckStmt(u_stmt, function_scope, return_type);
                }

                bool is_empty_tuple = *return_type == TupleRType(std::vector<std::shared_ptr<ResolvedType>>());

                if ((!is_empty_tuple) && (!has_return))
                    throw TypeException("Function " + result->function_name + " has a non-{} return type, but never returns.", cmd);
                return;
            }
        }
    }
    
    PseudoLValue::PseudoLValue(Parser::ASTNode* _replacement)
    {
        line = _replacement->line;
        pos = _replacement->pos;
        token_s = _replacement->token_s;
        replacement = _replacement;
    }

    PseudoTupleLValue::PseudoTupleLValue(std::vector<PseudoLValue*> _tuple, Parser::ASTNode* replacement) : PseudoLValue(replacement)
    {
        for(PseudoLValue* tuple_lvalue : _tuple)
        {
            lvalues.emplace_back(tuple_lvalue);
        }
    }

    std::pair<PseudoLValue*, std::shared_ptr<ResolvedType>> decompose_binding(std::unique_ptr<Parser::BindingNode>& u_binding, std::shared_ptr<Scope>& scope)
    {
        Parser::BindingNode* binding = u_binding.get();

        PseudoLValue* lvalue;
        std::shared_ptr<ResolvedType> rtype;
        
        // <argument> : <type>
        {
            Parser::VarBindingNode* result;
            if (tryCast<Parser::BindingNode, Parser::VarBindingNode>(binding, result))
            {
                lvalue = new PseudoArgumentLValue(result->argument.get(), binding);
                rtype = resolve_type(result->type, scope);
            }
        }
        // { <binding>, ... }
        {
            Parser::TupleBindingNode* result;
            if (tryCast<Parser::BindingNode, Parser::TupleBindingNode>(binding, result))
            {
                std::vector<PseudoLValue*> sub_lvalues;
                std::vector<std::shared_ptr<ResolvedType>> sub_rtypes;

                for (std::unique_ptr<Parser::BindingNode>& u_binding : result->bindings)
                {
                    std::pair<PseudoLValue*, std::shared_ptr<ResolvedType>> decomp = decompose_binding(u_binding, scope);
                    sub_lvalues.push_back(decomp.first);
                    sub_rtypes.push_back(decomp.second);
                }

                lvalue = new PseudoTupleLValue(sub_lvalues, binding);
                rtype = std::make_shared<TupleRType>(sub_rtypes);
            }
        }

        return std::pair<PseudoLValue*, std::shared_ptr<ResolvedType>>(lvalue, rtype);
    }

    bool typecheckStmt(std::unique_ptr<Parser::StmtNode>& u_stmt, std::shared_ptr<Scope>& scope, const std::shared_ptr<ResolvedType>& return_type)
    {
        Parser::StmtNode* stmt = u_stmt.get();

        // let <lvalue> = <expr>
        {
            Parser::LetStmtNode* result;
            if (tryCastStmt<Parser::LetStmtNode>(stmt, result))
            {
                std::shared_ptr<ResolvedType> rtype = type_of(result->variable_expression, scope);
                result->variable_expression->resolvedType = rtype;
                scope->add_lvalue(result->set_variable_name.get(), rtype);
                return false;
            }
        }

        // assert <expr> , <string>
        {
            Parser::AssertStmtNode* result;
            if (tryCastStmt<Parser::AssertStmtNode>(stmt, result))
            {
                std::shared_ptr<ResolvedType> rtype = type_of(result->expression, scope);
                if (rtype.get()->type_name != BOOL)
                    throw TypeException("Assert takes a boolean as its first argument. Detected an assert with an expression of type " + rtype->toString() + ".", result);
                result->expression->resolvedType = rtype;
                return false;
            }
        }

        // return <expr>
        {
            // Check to see if return type matches function signature.
            // There can be no return IF the the function returns {}
            Parser::ReturnStmtNode* result;
            if (tryCastStmt<Parser::ReturnStmtNode>(stmt, result))
            {
                std::shared_ptr<ResolvedType> rtype = type_of(result->expression, scope);
                if (*rtype != *return_type)
                    throw TypeException("Return type does not match type of function. Expected return of type " + return_type->toString() + ". Got " + rtype->toString() + ".", result);
                result->expression->resolvedType = rtype;
                return true;
            }
        }

        throw TypeException("Could not identify statement.", stmt);
    }
}