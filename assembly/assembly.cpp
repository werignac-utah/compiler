#include <algorithm>
#include <stdio.h>
#include "assembly.h"
#include "../trycasts.cpp"

namespace Compiler
{
    ////////////////////////////////////////
    ///             Exception            ///
    ////////////////////////////////////////

#pragma region Exception

    CompilerException::CompilerException(const std::string& m)
    {
        message = m;
    }

    const char* CompilerException::what() const noexcept
    {
        return message.c_str();
    }

#pragma endregion

    ////////////////////////////////////////
    ///             Assembly             ///
    ////////////////////////////////////////

    unsigned int calc_stack_size(std::shared_ptr<Typechecker::ResolvedType> resolved_type)
    {
        switch(resolved_type->type_name)
        {
            case Typechecker::INT:
            case Typechecker::FLOAT:
            case Typechecker::BOOL:
                return 8;
            case Typechecker::TUPLE:
                {
                    std::shared_ptr<Typechecker::TupleRType> resolved_as_tuple = std::static_pointer_cast<Typechecker::TupleRType>(resolved_type);
                    unsigned int sum_size = 0;
                    for (int i = 0; i < resolved_as_tuple->element_types.size(); i++)
                        sum_size += calc_stack_size(resolved_as_tuple->element_types[i]);
                    return sum_size;
                }
            case Typechecker::ARRAY:
                {
                    std::shared_ptr<Typechecker::ArrayRType> resolved_as_array = std::static_pointer_cast<Typechecker::ArrayRType>(resolved_type);
                    return 8 + 8 * resolved_as_array->rank;
                }
            default:
                throw CompilerException("Could not recognize type " + resolved_type->toString() + " for stack analysis."); 
        }
    }

#pragma region Assembly

    Assembly::Assembly(const Typechecker::Scope& scope, unsigned char _optimization_level) : optimization_level(_optimization_level)
    {
        for (const auto& kvp : scope.symbol_table)
        {
            std::string function_name = kvp.first;
            Typechecker::NameInfo* info = kvp.second.get();
            
            if (Typechecker::FuncInfo* func_info = dynamic_cast<Typechecker::FuncInfo*>(info))
            {
                CallingConvention cc(func_info->arguments, func_info->return_type);
                add_calling_convention(function_name, cc);
            }
        }

    }
    
#pragma region Add Constant

    std::string Assembly::add_constant_raw(std::string constant)
    {
        auto const_number = std::find(constants.begin(), constants.end(), constant);
        if (const_number == constants.end())
        {
            constants.push_back(constant);
            const_number = constants.end() - 1;
        }
        return "const" + std::to_string(const_number - constants.begin());//constants.begin()));
    }

    std::string Assembly::add_constant_string(std::string constant)
    {
        return add_constant_raw("db `" + constant + "`, 0");
    }

    std::string Assembly::add_constant_int(long constant)
    {
        return add_constant_raw("dq " + std::to_string(constant));
    }

    std::string Assembly::add_constant_float(double constant)
    {
        char buffer[50];
        std::sprintf(buffer, "dq %.10e", constant);
        return add_constant_raw(std::string(buffer));
    }

    std::string Assembly::add_constant_true()
    {
        return add_constant_raw("dq 1");
    }

    std::string  Assembly::add_constant_false()
    {
        return add_constant_raw("dq 0");
    }

#pragma endregion

    void Assembly::add_function(std::shared_ptr<IFunction> function)
    {
        functions.push_back(function);
    }

    std::string Assembly::get_new_jump()
    {
        return ".jump" + std::to_string(++jump_count);
    }

    std::string Assembly::toString()
    {
        std::string code = linkage_header + "\nsection .data\n";

        for (int const_number = 0; const_number < constants.size(); const_number++)
            code += "const" + std::to_string(const_number) + ": " + constants[const_number] + "\n";
        
        code += "\nsection .text\n";

        for (auto function : functions)
            code += function->toString();

        return code;
    }

    void Assembly::add_calling_convention(std::string name, CallingConvention cc)
    {
        calling_conventions.emplace(name, cc);
    }

    CallingConvention Assembly::get_calling_convention(std::string name)
    {
        // From https://stackoverflow.com/questions/19197799/what-is-the-quickest-way-of-inserting-updating-stdunordered-map-elements-witho
        auto it = calling_conventions.find(name);
        if (it == calling_conventions.end())
            throw CompilerException("Asked to access non-existant funciton " + name);
        return it->second;
    }

#pragma endregion

    ////////////////////////////////////////
    ///             Functions            ///
    ////////////////////////////////////////

#pragma region StackDescription

    void StackDescription::add_temporary(std::string name, const int offset)
    {
        temporaries[name] = offset;
    }

    void StackDescription::add_argument(Parser::ArgumentNode* argument, const std::shared_ptr<Typechecker::ResolvedType>& r_type, const int offset)
    {
        // <variable>
        {
            Parser::VarArgumentNode* result;
            if (tryCast<Parser::ArgumentNode, Parser::VarArgumentNode>(argument, result))
            {
                add_temporary(result->token_s, offset);
                return;
            }
        }
        
        // <variable> [ <variable> , ... ]
        {
            Parser::ArrayArgumentNode* result;
            if (tryCast<Parser::ArgumentNode, Parser::ArrayArgumentNode>(argument, result))
            {
                // regiser arguments where the array in the stack is:
                // -----
                // ptr in mem
                // number of elements in dimension n
                // number of elements in dimension n - 1
                // ...
                // number of elements in dimension 1
                // -----

                for (int i = 0; i < result->array_dimensions_names.size(); i++)
                {
                    std::string name = result->array_dimensions_names[i];                    
                    add_temporary(name, offset - 8 * i);
                }

                add_temporary(result->array_argument_name, offset);
                return;
            }
        }
    }

    void StackDescription::add_lvalue(Parser::LValue* lvalue, const std::shared_ptr<Typechecker::ResolvedType>& r_type, const int offset)
    {
        
        // <argument>
        {
            Parser::ArgumentLValue* result;
            if (tryCast<Parser::LValue, Parser::ArgumentLValue>(lvalue, result))
            {
                add_argument(result->argument.get(), r_type, offset);
                return;
            }
        }

        // pseudo arg
        {
            Typechecker::PseudoArgumentLValue* result;
            if (tryCast<Parser::LValue, Typechecker::PseudoArgumentLValue>(lvalue, result))
            {
                add_argument(result->argument, r_type, offset);
                return;
            }
        }

        // { <lvalue>, }
        {
            Parser::TupleLValueNode* result;
            if (tryCast<Parser::LValue, Parser::TupleLValueNode>(lvalue, result))
            {
                unsigned int next_offset = offset;
                std::shared_ptr<Typechecker::TupleRType> tuple_rtype = std::static_pointer_cast<Typechecker::TupleRType>(r_type);

                for (int i = 0; i < result->lvalues.size(); i++)
                {
                    Parser::LValue* sub_lvalue = result->lvalues[i].get();
                    std::shared_ptr<Typechecker::ResolvedType> sub_rtype = tuple_rtype->element_types[i];
                    add_lvalue(sub_lvalue, sub_rtype, next_offset);
                    next_offset -= calc_stack_size(sub_rtype);
                }
                return;
            }
        }

        // pseudo tuple
        {
            Typechecker::PseudoTupleLValue* result;
            if (tryCast<Parser::LValue, Typechecker::PseudoTupleLValue>(lvalue, result))
            {
                unsigned int next_offset = offset;
                std::shared_ptr<Typechecker::TupleRType> tuple_rtype = std::static_pointer_cast<Typechecker::TupleRType>(r_type);

                for (int i = 0; i < result->lvalues.size(); i++)
                {
                    Parser::LValue* sub_lvalue = result->lvalues[i].get();
                    std::shared_ptr<Typechecker::ResolvedType> sub_rtype = tuple_rtype->element_types[i];
                    add_lvalue(sub_lvalue, sub_rtype, next_offset);
                    next_offset -= calc_stack_size(sub_rtype);
                }
                return;
            }
        }
    }

    void StackDescription::add_binding(Parser::BindingNode* binding, const std::shared_ptr<Typechecker::ResolvedType>& r_type, const int offset)
    {
        // <argument> : <type>
        {
            Parser::VarBindingNode* result;
            if (tryCast<Parser::BindingNode, Parser::VarBindingNode>(binding, result))
            {
                add_argument(result->argument.get(), r_type, offset);
                return;
            }
        }

        // {<binding>, <binding>, ...}
        {
            Parser::TupleBindingNode* result;
            if (tryCast<Parser::BindingNode, Parser::TupleBindingNode>(binding, result))
            {
                unsigned int sub_offset = offset;

                Typechecker::TupleRType* tuple = static_cast<Typechecker::TupleRType*>(r_type.get());

                for (int i = 0; i < result->bindings.size(); i++)
                {
                    std::unique_ptr<Parser::BindingNode>& sub_binding = result->bindings[i];
                    std::shared_ptr<Typechecker::ResolvedType> sub_type = tuple->element_types[i];
                    add_binding(sub_binding.get(), sub_type, sub_offset);
                    sub_offset -= calc_stack_size(sub_type);
                }
                return;
            }
        }
    }

#pragma endregion

#pragma region CallingConvention

    bool CallingConvention::is_integral(std::shared_ptr<Typechecker::ResolvedType> r_type)
    {
        return r_type->type_name == Typechecker::INT || r_type->type_name == Typechecker::BOOL;
    }

    bool CallingConvention::is_float(std::shared_ptr<Typechecker::ResolvedType> r_type)
    {
        return r_type->type_name == Typechecker::FLOAT;
    }

    bool CallingConvention::is_agregate(std::shared_ptr<Typechecker::ResolvedType> r_type)
    {
        return r_type->type_name == Typechecker::TUPLE || r_type->type_name == Typechecker::ARRAY; 
    }

    bool CallingConvention::is_void_return_type(std::shared_ptr<Typechecker::ResolvedType> r_type)
    {
        std::vector<std::shared_ptr<Typechecker::ResolvedType>> _empty;
        Typechecker::TupleRType void_return(_empty);
        return *r_type == void_return;
    }

    std::string CallingConvention::get_register_name(MemoryLocation loc)
    {
        switch (loc)
        {
        case RDI:
            return "rdi";
        case RSI:
            return "rsi";
        case RDX:
            return "rdx";
        case RCX:
            return "rcx";
        case R8:
            return "r8";
        case R9:
            return "r9";
        case XMM0:
            return "xmm0";
        case XMM1:
            return "xmm1";
        case XMM2:
            return "xmm2";
        case XMM3:
            return "xmm3";
        case XMM4:
            return "xmm4";
        case XMM5:
            return "xmm5";
        case XMM6:
            return "xmm6";
        case XMM7:
            return "xmm7";
        case RAX:
            return "rax";
        default:
            break;
        }

        throw CompilerException("Asked for register name of a non-register data location.");
    }

    bool CallingConvention::is_r_register(MemoryLocation loc)
    {
        int intval = static_cast<int>(loc);
        return (intval >= 0 && intval < R_REGISTER_COUNT) || intval == RAX; 
    }

    bool CallingConvention::is_f_register(MemoryLocation loc)
    {
        int intval = static_cast<int>(loc);
        return (intval >= R_REGISTER_COUNT && intval < R_REGISTER_COUNT + F_REGISTER_COUNT);
    }

    CallingConvention::CallingConvention(const std::vector<std::shared_ptr<Typechecker::ResolvedType>>& arguments, const std::shared_ptr<Typechecker::ResolvedType>& return_type)
    {
        for (const std::shared_ptr<Typechecker::ResolvedType>& r_type : arguments)
            arg_signature.push_back(r_type);
        
        ret_signature = return_type;

        stack_argument_size = 0;
        return_size = 0;
        
        // Construct a stack and some arrays for the registers. Maybe also keep a vector for the final positions of everything.
        int next_free_r_register = 0;
        int next_free_f_register = 0;
        std::vector<MemoryLocationData> register_arguments;
        std::vector<MemoryLocationData> stack;
        
        // if the return type is an agregate type, reserve RDI and place the return on the stack TOP. Otherwise, put in RAX.
        is_void_return = is_void_return_type(return_type);
        
        if (! is_void_return)
        {
            switch(return_type->type_name)
            {
            case Typechecker::INT:
            case Typechecker::BOOL:
                return_location = RAX;
                break;
            case Typechecker::FLOAT:
                return_location = XMM0;
                break;
            case Typechecker::ARRAY:
            case Typechecker::TUPLE:
                return_location = STACK;
                return_size = calc_stack_size(return_type);
                next_free_r_register++; //RDI taken up by return.
                break;
            }
        }

        // for each argument
        // if it's an integral, try adding to the r registers. If none are available, push to stack TOP BENEATH RETURN.
        // if it's a float, try adding to the xmm registers. If none are available, push to stack TOP BENEATH RETURN.
        // if it's an agregate, push to stack TOP BENEATH RETURN.

        for (int i = 0; i < arguments.size(); i++)
        {
            std::shared_ptr<Typechecker::ResolvedType> arg_type = arguments[i];

            if (is_integral(arg_type) && next_free_r_register < R_REGISTER_COUNT)
            {
                MemoryLocationData data {static_cast<MemoryLocation>(next_free_r_register), i};
                register_arguments.push_back(data);
                next_free_r_register++;
            }
            else if (is_float(arg_type) && next_free_f_register < F_REGISTER_COUNT)
            {
                MemoryLocationData data {static_cast<MemoryLocation>(R_REGISTER_COUNT + next_free_f_register), i};
                register_arguments.push_back(data);
                next_free_f_register++;
            }
            else
            {
                MemoryLocationData data {STACK, i};
                stack.push_back(data);
                stack_argument_size += calc_stack_size(arg_type);
            }
        }

        // create an order for the arguments to be pushed in and store order.
        argument_pop_order.insert(argument_pop_order.end(), register_arguments.begin(), register_arguments.end());
        argument_pop_order.insert(argument_pop_order.end(), stack.begin(), stack.end());
    }

#pragma endregion

#pragma region Functions

    AFunction::AFunction(Parser::FnCmd* cmd, Assembly& _assembly, StackDescription* _global_stack) : name(cmd->function_name), assembly(_assembly), is_main(false), stack_size(0), global_stack(_global_stack)
    {
        // Generate argument temporaries
        CallingConvention cc = assembly.get_calling_convention(name);
        
        int stack_args_dist_from_rbp = -16;
        
        if (! cc.is_void_return && cc.return_location == CallingConvention::STACK)
        {
            assembly_code.push_back("push rdi ; $return");
            stack_size += 8;
            stack_size.add_temporary("$return", stack_size.get_size_of_temporaries());
        }

        for (int i = 0; i < cc.argument_pop_order.size(); i++)
        {
            const CallingConvention::MemoryLocationData& data = cc.argument_pop_order[i];
            Parser::BindingNode* binding_node = cmd->arguments[data.argument_number].get();
            std::shared_ptr<Typechecker::ResolvedType> binding_type = cc.arg_signature[data.argument_number];


            if (CallingConvention::is_r_register(data.location))
            {
                assembly_code.push_back("push " + CallingConvention::get_register_name(data.location));
                stack_size += 8;
                stack_size.add_binding(binding_node, binding_type, stack_size.get_size_of_temporaries());
            }
            else if (CallingConvention::is_f_register(data.location))
            {
                assembly_code.push_back("sub rsp, 8");
                stack_size += 8;
                assembly_code.push_back("movsd [rsp], " + CallingConvention::get_register_name(data.location));
                stack_size.add_binding(binding_node, binding_type, stack_size.get_size_of_temporaries());
            }
            else
            {
                stack_size.add_binding(binding_node, binding_type, stack_args_dist_from_rbp);
                stack_args_dist_from_rbp -=  (int) calc_stack_size(binding_type);
            }
        }

        // Process Statements

        bool had_return = false;

        for (const std::unique_ptr<Parser::StmtNode>& stmt : cmd->function_contents)
        {
            had_return = cg_stmt(stmt.get(), cc) || had_return;
        }

        if (! had_return)
            add_function_return_code(cc);
    }

#pragma region Commands

    void AFunction::cg_cmd(std::unique_ptr<Parser::CmdNode>& cmd)
    {
        Parser::CmdNode* cmd_ptr = cmd.get();
        
        {
            Parser::ShowCmdNode* result;
            if (tryCastCmd<Parser::ShowCmdNode>(cmd_ptr, result))
            {
                cg_showcmd(result);
                return;
            }
        }

        {
            Parser::LetCmdNode* result;
            if (tryCastCmd<Parser::LetCmdNode>(cmd_ptr, result))
            {
                cg_letcmd(result);
                return;
            }
        }

        {
            Parser::ReadCmdNode* result;
            if (tryCastCmd<Parser::ReadCmdNode>(cmd_ptr, result))
            {
                cg_readcmd(result);
                return;
            }
        }

        {
            Parser::FnCmd* result;
            if (tryCastCmd<Parser::FnCmd>(cmd_ptr, result))
            {
                cg_fncmd(result);
                return;
            }
        }

        {
            Parser::AssertCmdNode* result;
            if (tryCastCmd<Parser::AssertCmdNode>(cmd_ptr, result))
            {
                cg_assertcmd(result);
                return;
            }
        }

        {
            Parser::TypeCmdNode* result;
            if (tryCastCmd<Parser::TypeCmdNode>(cmd_ptr, result))
            {
                return;
            }
        }

        {
            Parser::PrintCmdNode* result;
            if (tryCastCmd<Parser::PrintCmdNode>(cmd_ptr, result))
            {
                cg_printcmd(result);
                return;
            }
        }

        {
            Parser::WriteCmdNode* result;
            if (tryCastCmd<Parser::WriteCmdNode>(cmd_ptr, result))
            {
                cg_writecmd(result);
                return;
            }
        }

        {
            Parser::TimeCmdNode* result;
            if (tryCastCmd<Parser::TimeCmdNode>(cmd_ptr, result))
            {
                cg_timecmd(result);
                return;
            }
        }

        throw CompilerException("Unrecognized command " + cmd->token_s + ".");
    }

#define FUNCTION_CALL_ALIGNMENT_CHECK(argument_size_on_stack) bool needs_alignment = (stack_size.get_stack_size() + argument_size_on_stack) % 16 != 0; if (needs_alignment){assembly_code.push_back("sub rsp, 8 ;align stack");stack_size.increment_stack_size(8);}
#define FUNCTION_CALL_ALIGNMENT_CLOSE if (needs_alignment){assembly_code.push_back("add rsp, 8 ;undo alignment");stack_size.decrement_stack_size(8);}
    void AFunction::cg_showcmd(Parser::ShowCmdNode* cmd)
    {
        unsigned int argument_size_on_stack = calc_stack_size(cmd->expression->resolvedType);

        FUNCTION_CALL_ALIGNMENT_CHECK(argument_size_on_stack)

        cg_expr(cmd->expression);

        assembly_code.push_back("; " + cmd->token_s + " | line: " + std::to_string(cmd->line));

        std::string expression_type = "(" + cmd->expression->resolvedType->toString() + ")";
        std::string expression_constant_name  = assembly.add_constant_string(expression_type);

        assembly_code.push_back("lea rdi, [rel " + expression_constant_name + "] ; " + expression_type);
        assembly_code.push_back("lea rsi, [rsp]");
        assembly_code.push_back("call _show");
        assembly_code.push_back("add rsp, " + std::to_string(argument_size_on_stack));
        stack_size.decrement_stack_size(argument_size_on_stack);

        FUNCTION_CALL_ALIGNMENT_CLOSE
    }

    void AFunction::cg_letcmd(Parser::LetCmdNode* cmd)
    {
        cg_expr(cmd->expression);
        assembly_code.push_back("; " + cmd->token_s + " | line: " + std::to_string(cmd->line));
        stack_size.add_lvalue(cmd->lvalue.get(), cmd->expression->resolvedType, stack_size.get_size_of_temporaries());
    }

    void AFunction::cg_readcmd(Parser::ReadCmdNode* cmd)
    {
        std::shared_ptr<Typechecker::FloatRType> r_float = std::make_shared<Typechecker::FloatRType>();
        std::vector<std::shared_ptr<Typechecker::ResolvedType>> r_floats {r_float, r_float, r_float, r_float};
        std::shared_ptr<Typechecker::ResolvedType> r_tuple = std::make_shared<Typechecker::TupleRType>(r_floats);
        std::shared_ptr<Typechecker::ResolvedType> r_pict = std::make_shared<Typechecker::ArrayRType>(r_tuple, 2);
        
        unsigned int return_size_on_stack = calc_stack_size(r_pict); // Size of Array Image in Stack
        stack_size += return_size_on_stack;
        

        assembly_code.push_back("; " + cmd->token_s + " | line: " + std::to_string(cmd->line));
        assembly_code.push_back("sub rsp, " + std::to_string(return_size_on_stack));
        assembly_code.push_back("lea rdi, [rsp]");
        FUNCTION_CALL_ALIGNMENT_CHECK(0)
        std::string const_name = assembly.add_constant_string(cmd->fileName->getValue());
        assembly_code.push_back("lea rsi, [rel " + const_name + "] ; " + cmd->fileName->getValue());
        assembly_code.push_back("call _read_image");
        FUNCTION_CALL_ALIGNMENT_CLOSE
        stack_size.add_argument(cmd->readInto.get(),  r_pict, stack_size.get_size_of_temporaries());
    }

    void AFunction::cg_fncmd(Parser::FnCmd* cmd)
    {
        std::shared_ptr<IFunction> sub_function = std::make_shared<AFunction>(cmd, assembly, global_stack);
        assembly.add_function(sub_function);
    }

    void AFunction::cg_assertcmd(Parser::AssertCmdNode* cmd)
    {
        cg_expr(cmd->expression);
        assembly_code.push_back("pop rax");
        stack_size -= 8;
        assembly_code.push_back("cmp rax, 0 ; check assert");
        std::string jump_name = assembly.get_new_jump();
        assembly_code.push_back("jne " + jump_name);
        FUNCTION_CALL_ALIGNMENT_CHECK(0)
        std::string error_message_constant = assembly.add_constant_string(cmd->string->getValue());
        assembly_code.push_back("lea rdi, [rel " + error_message_constant + "] ; " + cmd->string->getValue());
        assembly_code.push_back("call _fail_assertion");
        FUNCTION_CALL_ALIGNMENT_CLOSE
        assembly_code.push_back(jump_name + ":");
    }

    void AFunction::cg_printcmd(Parser::PrintCmdNode* cmd)
    {
        std::string message_const = assembly.add_constant_string(cmd->string->getValue());
        assembly_code.push_back("lea rdi, [rel " + message_const + "] ; " + cmd->string->getValue());
        FUNCTION_CALL_ALIGNMENT_CHECK(0)
        assembly_code.push_back("call _print ; print " + cmd->string->getValue());
        FUNCTION_CALL_ALIGNMENT_CLOSE
    }

    void AFunction::cg_writecmd(Parser::WriteCmdNode* cmd)
    {
        unsigned int size_of_image_on_stack = 24;
        FUNCTION_CALL_ALIGNMENT_CHECK(size_of_image_on_stack)
        cg_expr(cmd->toSave);
        std::string filename_const = assembly.add_constant_string(cmd->fileName->getValue());
        assembly_code.push_back("lea rdi, [rel " + filename_const + "] ; " + cmd->fileName->getValue());
        assembly_code.push_back("call _write_image ; " + cmd->token_s);
        assembly_code.push_back("add rsp, " + std::to_string(size_of_image_on_stack));
        stack_size -= size_of_image_on_stack;
        FUNCTION_CALL_ALIGNMENT_CLOSE
    }

    void AFunction::cg_timecmd(Parser::TimeCmdNode* cmd)
    {
        assembly_code.push_back("; Timing call to " + cmd->command->token_s);
        {
        FUNCTION_CALL_ALIGNMENT_CHECK(0)
        assembly_code.push_back("call _get_time ; getting pre-op time");
        FUNCTION_CALL_ALIGNMENT_CLOSE
        }
        assembly_code.push_back("sub rsp, 8");
        stack_size += 8;
        assembly_code.push_back("movsd [rsp], xmm0 ; collecting _get_time return");

        unsigned int start_offset = stack_size.get_stack_size();

        cg_cmd(cmd->command);

        {
        FUNCTION_CALL_ALIGNMENT_CHECK(0)
        assembly_code.push_back("call _get_time ; getting post-op time");
        FUNCTION_CALL_ALIGNMENT_CLOSE
        }
        assembly_code.push_back("sub rsp, 8");
        stack_size += 8;
        assembly_code.push_back("movsd [rsp], xmm0 ; collecting _get_time return");

        assembly_code.push_back("movsd xmm0, [rsp] ; end time");
        assembly_code.push_back("add rsp, 8");
        stack_size -= 8;
        unsigned int end_offset = stack_size.get_stack_size();
        assembly_code.push_back("movsd xmm1, [rsp + " + std::to_string(end_offset - start_offset) + "] ; start time");

        assembly_code.push_back("subsd xmm0, xmm1 ; op time = end - start");
        {
        FUNCTION_CALL_ALIGNMENT_CHECK(0)
        assembly_code.push_back("call _print_time");
        FUNCTION_CALL_ALIGNMENT_CLOSE
        }
    }

#pragma endregion

#pragma region Expressions
    void AFunction::cg_expr(std::unique_ptr<Parser::ExprNode>& expr)
    {
        Parser::ExprNode* expr_ptr = expr.get();

        /* OPTIMIZES TOO MUCH
        {
            if (assembly.get_optimization_level() > 1 && expr_ptr->cp->type == Parser::CPValue::INT)
            {
                Parser::IntValue* as_int = static_cast<Parser::IntValue*>(expr_ptr->cp.get());
                cg_push_constant_int(as_int->value);
                return;
            }
        }*/

#pragma region Primitive Literals Casting
        
        {
            Parser::IntExprNode* result;
            if (tryCastExpr<Parser::IntExprNode>(expr_ptr, result))
            {
                cg_intexpr(result);
                return;
            }
        }

        {
            Parser::FloatExprNode* result;
            if (tryCastExpr<Parser::FloatExprNode>(expr_ptr, result))
            {
                cg_floatexpr(result);
                return;
            }
        }

        {
            Parser::TrueExprNode* result;
            if (tryCastExpr<Parser::TrueExprNode>(expr_ptr, result))
            {
                cg_trueexpr(result);
                return;
            }
        }

        {
            Parser::FalseExprNode* result;
            if (tryCastExpr<Parser::FalseExprNode>(expr_ptr, result))
            {
                cg_falseexpr(result);
                return;
            }
        }

#pragma endregion
        
#pragma region Operations Casting
        
        {
            Parser::UnopExprNode* result;
            if (tryCastExpr<Parser::UnopExprNode>(expr_ptr, result))
            {
                cg_unopexpr(result);
                return;
            }
        }

        {
            Parser::BinopExprNode* result;
            if (tryCastExpr<Parser::BinopExprNode>(expr_ptr, result))
            {
                cg_binopexpr(result);
                return;
            }
        }

#pragma endregion

#pragma region Tuple and Array Literals Casting

        {
            Parser::TupleLiteralExprNode* result;
            if (tryCastExpr<Parser::TupleLiteralExprNode>(expr_ptr, result))
            {
                cg_tupleexpr(result);
                return;
            }
        }

        {
            Parser::ArrayLiteralExprNode* result;
            if (tryCastExpr<Parser::ArrayLiteralExprNode>(expr_ptr, result))
            {
                cg_arrayexpr(result);
                return;
            }
        }

#pragma endregion

#pragma region Tuple and Array Accesses Casting

        {
            Parser::TupleIndexExprNode* result;
            if (tryCastExpr<Parser::TupleIndexExprNode>(expr_ptr, result))
            {
                cg_tupleaccessexpr(result);
                return;
            }
        }

        {
            Parser::ArrayIndexExprNode* result;
            if (tryCastExpr<Parser::ArrayIndexExprNode>(expr_ptr, result))
            {
                cg_arrayindexexpr(result);
                return;
            }
        }

#pragma endregion

        {
            Parser::VariableExprNode* result;
            if (tryCastExpr<Parser::VariableExprNode>(expr_ptr, result))
            {
                cg_variableexpr(result);
                return;
            }
        }

        {
            Parser::CallExprNode* result;
            if (tryCastExpr<Parser::CallExprNode>(expr_ptr, result))
            {
                cg_callexpr(result);
                return;
            }
        }

        {
            Parser::IfExprNode* result;
            if (tryCastExpr<Parser::IfExprNode>(expr_ptr, result))
            {
                cg_ifexpr(result);
                return;
            }
        }

        {
            Parser::LoopExprNode* result;
            if (tryCastExpr<Parser::LoopExprNode>(expr_ptr, result))
            {
                cg_loopexpr(result);
                return;
            }
        }

        throw CompilerException("Unrecognized expression " + expr->token_s + ".");
    }

#pragma region Primitive Literals

    void AFunction::cg_intexpr(Parser::IntExprNode* expr)
    {
        cg_push_constant_int(expr->value);
    }

    void AFunction::cg_floatexpr(Parser::FloatExprNode* expr)
    {
        std::string const_name = assembly.add_constant_float(expr->value);
        assembly_code.push_back("mov rax, [rel "+ const_name +"] ; " + std::to_string(expr->value));
        assembly_code.push_back("push rax");
        stack_size.increment_stack_size(8);
    }

    void AFunction::cg_trueexpr(Parser::TrueExprNode* expr)
    {
        cg_push_constant_int(1, "true");
    }

    void AFunction::cg_falseexpr(Parser::FalseExprNode* expr)
    {
        cg_push_constant_int(0, "false");
    }

#pragma endregion

#pragma region Operations

    void AFunction::cg_unopexpr(Parser::UnopExprNode* expr)
    {
        cg_expr(expr->expression);
        assembly_code.push_back("; " + expr->token_s);

        switch(expr->operation)
        {
            case Parser::UnopExprNode::NEGATION:
                {
                    switch (expr->expression->resolvedType->type_name)
                    {
                        case Typechecker::INT:
                            assembly_code.push_back("pop rax");
                            stack_size -= 8;
                            assembly_code.push_back("neg rax");
                            assembly_code.push_back("push rax");
                            stack_size += 8;
                            break;
                        case Typechecker::FLOAT:
                            
                            assembly_code.push_back("movsd xmm1, [rsp]");
                            assembly_code.push_back("add rsp, 8");                    
                            stack_size -= 8;
                            assembly_code.push_back("pxor xmm0, xmm0");
                            assembly_code.push_back("subsd xmm0, xmm1");
                            assembly_code.push_back("sub rsp, 8");
                            stack_size += 8;
                            assembly_code.push_back("movsd [rsp], xmm0");
                            break;
                        default:
                            throw CompilerException("Unrecognized type for negation operation " + expr->token_s + ". Expected an int or float.");
                    }
                }
                return;
            case Parser::UnopExprNode::NOT:
                if (expr->expression->resolvedType->type_name != Typechecker::BOOL)
                    throw CompilerException("Unrecognized type for not operation " + expr->token_s + ". Expected a boolean.");
                // Assume unop expression is a boolean type.
                assembly_code.push_back("pop rax");
                stack_size -= 8;
                assembly_code.push_back("xor rax, 1");
                assembly_code.push_back("push rax");
                stack_size += 8;
                return;
        }

        throw CompilerException("Unrecognized unary operation " + expr->token_s + ".");
    }

#define BINOP_PRINT assembly_code.push_back("; " + expr->token_s);
#define BINOP_GET_TWO_ARGS cg_expr(expr->rhs); cg_expr(expr->lhs); BINOP_PRINT

#define BINOP_GET_TWO_INT_ARGS BINOP_GET_TWO_ARGS assembly_code.push_back("pop rax"); stack_size -= 8; assembly_code.push_back("pop r10"); stack_size -= 8;
#define BINOP_GET_TWO_FLOATS_ARGS BINOP_GET_TWO_ARGS assembly_code.push_back("movsd xmm0, [rsp]"); assembly_code.push_back("add rsp, 8"); stack_size -= 8; assembly_code.push_back("movsd xmm1, [rsp]"); assembly_code.push_back("add rsp, 8"); stack_size -=8;

    void AFunction::cg_binopexpr(Parser::BinopExprNode* expr)
    {
        switch (expr->operation)
        {
            case Parser::BinopExprNode::AND:
            case Parser::BinopExprNode::OR:
                cg_shortcircuit(expr);
                return;
            default:
                break;
        }

        switch (expr->operation)
        {
            case Parser::BinopExprNode::PLUS:
                switch (expr->resolvedType->type_name)
                {
                    case Typechecker::INT:
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("add rax, r10");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("addsd xmm0, xmm1");
                        assembly_code.push_back("sub rsp, 8");
                        stack_size += 8;
                        assembly_code.push_back("movsd [rsp], xmm0");
                        return;
                    default:
                        throw CompilerException("Unrecognized type for plus operation " + expr->token_s + ". Expected an int or float.");
                }
                break;
            case Parser::BinopExprNode::MINUS:
                switch (expr->resolvedType->type_name)
                {
                    case Typechecker::INT:
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("sub rax, r10");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("subsd xmm0, xmm1");
                        assembly_code.push_back("sub rsp, 8");
                        stack_size += 8;
                        assembly_code.push_back("movsd [rsp], xmm0");
                        return;
                    default:
                        throw CompilerException("Unrecognized type for minus operation " + expr->token_s + ". Expected an int or float.");
                }
                break;
            case Parser::BinopExprNode::TIMES:
                switch (expr->resolvedType->type_name)
                {
                    case Typechecker::INT:

                        if (assembly.get_optimization_level() == 1) // multiply to shift left logical for powers of 2
                        {
                            Parser::ExprNode* lhs = expr->lhs.get();
                            Parser::IntExprNode* lhs_cast;
                            bool lhs_casted = tryCastExpr<Parser::IntExprNode>(lhs, lhs_cast);
                            long power;
                            long multiply_amount = 0;

                            bool optimized = false;

                            if (lhs_casted && (is_power_of_two(lhs_cast->value, power) && lhs_cast->value != 0))
                            {
                                multiply_amount = lhs_cast->value;
                                if (multiply_amount != 0)
                                    cg_expr(expr->rhs);
                                optimized = true;
                            }

                            if (! optimized)
                            {
                                Parser::ExprNode* rhs = expr->rhs.get();
                                Parser::IntExprNode* rhs_cast;
                                bool rhs_casted = tryCastExpr<Parser::IntExprNode>(rhs, rhs_cast);

                                if (rhs_casted && (is_power_of_two(rhs_cast->value, power) && rhs_cast->value != 0))
                                {
                                    multiply_amount = rhs_cast->value;
                                    if (multiply_amount != 0)
                                        cg_expr(expr->lhs);
                                    optimized = true;
                                }
                            }

                            if (optimized && power != 0)
                            {
                                BINOP_PRINT
                                assembly_code.push_back("pop rax");
                                stack_size -= 8;
                                assembly_code.push_back("shl rax, " + std::to_string(power));
                                assembly_code.push_back("push rax");
                                stack_size += 8;
                                return;
                            }
                            else if (power == 0 && multiply_amount == 1)
                            {
                                return;
                            }
                        }

                        if (assembly.get_optimization_level() > 1) // multiply to shift left logical for powers of 2 CPValues
                        {
                            bool optimized = false;
                            long power;
                            long multiply_amount = 0;

                            Parser::CPValue* lhs = expr->lhs->cp.get();
                            bool can_cast_lhs = lhs->type == Parser::CPValue::INT;
                            
                            if (can_cast_lhs)
                            {
                                Parser::IntValue* lhs_cast = static_cast<Parser::IntValue*>(lhs); 

                                if (is_power_of_two(lhs_cast->value, power) && lhs_cast->value != 0)
                                {
                                    multiply_amount = lhs_cast->value;
                                    if (multiply_amount != 0)
                                        cg_expr(expr->rhs);
                                    optimized = true;
                                }
                            }

                            if (! optimized)
                            {
                                Parser::CPValue* rhs = expr->rhs->cp.get();
                                bool can_cast_rhs = rhs->type == Parser::CPValue::INT;
                                
                                if (can_cast_rhs)
                                {
                                    Parser::IntValue* rhs_cast = static_cast<Parser::IntValue*>(rhs);

                                    if (is_power_of_two(rhs_cast->value, power) && rhs_cast->value != 0)
                                    {
                                        multiply_amount = rhs_cast->value;
                                        if (multiply_amount != 0)
                                            cg_expr(expr->lhs);
                                        optimized = true;
                                    }
                                }
                            }

                            if (optimized && power != 0)
                            {
                                BINOP_PRINT
                                assembly_code.push_back("pop rax");
                                stack_size -= 8;
                                assembly_code.push_back("shl rax, " + std::to_string(power));
                                assembly_code.push_back("push rax");
                                stack_size += 8;
                                return;
                            }
                            else if (power == 0 && multiply_amount == 1)
                            {
                                return;
                            }
                        }

                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("imul rax, r10");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("mulsd xmm0, xmm1");
                        assembly_code.push_back("sub rsp, 8");
                        stack_size += 8;
                        assembly_code.push_back("movsd [rsp], xmm0");
                        return;
                    default:
                        throw CompilerException("Unrecognized type for multiply operation " + expr->token_s + ". Expected an int or float.");
                }
                break;
            case Parser::BinopExprNode::DIVIDE:
                switch (expr->resolvedType->type_name)
                {
                    case Typechecker::INT:
                        {
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("cmp r10, 0 ; check for division by zero");
                        std::string jump_name = assembly.get_new_jump();
                        assembly_code.push_back("jne " + jump_name);
                        FUNCTION_CALL_ALIGNMENT_CHECK(0)
                        std::string error_message_constant = assembly.add_constant_string("divide by zero");
                        assembly_code.push_back("lea rdi, [rel " + error_message_constant + "] ; divide by zero");
                        assembly_code.push_back("call _fail_assertion");
                        FUNCTION_CALL_ALIGNMENT_CLOSE
                        assembly_code.push_back(jump_name + ":");
                        assembly_code.push_back("cqo");
                        assembly_code.push_back("idiv r10");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                        }
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("divsd xmm0, xmm1");
                        assembly_code.push_back("sub rsp, 8");
                        stack_size += 8;
                        assembly_code.push_back("movsd [rsp], xmm0");
                        return;
                    default:
                        throw CompilerException("Unrecognized type for divide operation " + expr->token_s + ". Expected an int or float.");
                }
                break;
            case Parser::BinopExprNode::MOD:
                switch (expr->resolvedType->type_name)
                {
                    case Typechecker::INT:
                        {
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("cmp r10, 0 ; check for mod by zero");
                        std::string jump_name = assembly.get_new_jump();
                        assembly_code.push_back("jne " + jump_name);
                        FUNCTION_CALL_ALIGNMENT_CHECK(0)
                        std::string error_message_constant = assembly.add_constant_string("mod by zero");
                        assembly_code.push_back("lea rdi, [rel " + error_message_constant + "] ; divide by zero");
                        assembly_code.push_back("call _fail_assertion");
                        FUNCTION_CALL_ALIGNMENT_CLOSE
                        assembly_code.push_back(jump_name + ":");
                        assembly_code.push_back("cqo");
                        assembly_code.push_back("idiv r10");
                        assembly_code.push_back("mov rax, rdx"); // line of code that makes mod != division
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                        }
                    case Typechecker::FLOAT:
                        {
                        //FUNCTION_CALL_ALIGNMENT_CHECK(-16)
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("call _fmod");
                        assembly_code.push_back("sub rsp, 8");
                        stack_size += 8;
                        assembly_code.push_back("movsd [rsp], xmm0");
                        //FUNCTION_CALL_ALIGNMENT_CLOSE
                        return;
                        }
                    default:
                        throw CompilerException("Unrecognized type for mod operation " + expr->token_s + ". Expected an int or float.");
                }
                break;
            case Parser::BinopExprNode::LESS_THAN:
                switch (expr->lhs->resolvedType->type_name)
                {
                    case Typechecker::INT:
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("cmp rax, r10");
                        assembly_code.push_back("setl al"); // less than
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("cmpltsd xmm0, xmm1"); // less than
                        assembly_code.push_back("movq rax, xmm0");
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    default:
                        throw CompilerException("Unrecognized type for mod operation " + expr->token_s + ". Expected an int or float.");
                }
                break;
            case Parser::BinopExprNode::GREATER_THAN:
                switch (expr->lhs->resolvedType->type_name)
                {
                    case Typechecker::INT:
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("cmp rax, r10");
                        assembly_code.push_back("setg al"); // greater than
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("cmpltsd xmm1, xmm0"); // greater than
                        assembly_code.push_back("movq rax, xmm1");
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    default:
                        throw CompilerException("Unrecognized type for mod operation " + expr->token_s + ". Expected an int or float.");
                }
                break;
            case Parser::BinopExprNode::EQUALS:
                switch (expr->lhs->resolvedType->type_name)
                {
                    case Typechecker::BOOL:
                    case Typechecker::INT:
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("cmp rax, r10");
                        assembly_code.push_back("sete al"); // equals
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("cmpeqsd xmm0, xmm1"); // equal
                        assembly_code.push_back("movq rax, xmm0");
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    default:
                        throw CompilerException("Unrecognized type for mod operation " + expr->token_s + ". Expected an int or float.");
                }
            case Parser::BinopExprNode::NOT_EQUALS:
                switch (expr->lhs->resolvedType->type_name)
                {
                    case Typechecker::BOOL:
                    case Typechecker::INT:
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("cmp rax, r10");
                        assembly_code.push_back("setne al"); // not equals
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("cmpneqsd xmm0, xmm1"); // not equal
                        assembly_code.push_back("movq rax, xmm0");
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    default:
                        throw CompilerException("Unrecognized type for mod operation " + expr->token_s + ". Expected an int or float.");
                }
            case Parser::BinopExprNode::LESS_THAN_OR_EQUALS:
                switch (expr->lhs->resolvedType->type_name)
                {
                    case Typechecker::INT:
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("cmp rax, r10");
                        assembly_code.push_back("setle al"); // less than or equal
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("cmplesd xmm0, xmm1"); // less than or equal
                        assembly_code.push_back("movq rax, xmm0");
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    default:
                        throw CompilerException("Unrecognized type for mod operation " + expr->token_s + ". Expected an int or float.");
                }
                break;
            case Parser::BinopExprNode::GREATER_THAN_OR_EQUALS:
                switch (expr->lhs->resolvedType->type_name)
                {
                    case Typechecker::INT:
                        BINOP_GET_TWO_INT_ARGS
                        assembly_code.push_back("cmp rax, r10");
                        assembly_code.push_back("setge al"); // greater than or equal
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    case Typechecker::FLOAT:
                        BINOP_GET_TWO_FLOATS_ARGS
                        assembly_code.push_back("cmplesd xmm1, xmm0"); // greater than or equal
                        assembly_code.push_back("movq rax, xmm1");
                        assembly_code.push_back("and rax, 1");
                        assembly_code.push_back("push rax");
                        stack_size += 8;
                        return;
                    default:
                        throw CompilerException("Unrecognized type for mod operation " + expr->token_s + ". Expected an int or float.");
                }
                break;
            default:
                break;
        }

        throw CompilerException("Unrecognized binop operation " + expr->token_s + ".");
    }

#pragma endregion

#pragma region Tuple and Array Literals

    void AFunction::cg_tupleexpr(Parser::TupleLiteralExprNode* expr)
    {
        for (int i = expr->tuple_expressions.size() - 1; i >= 0; i--)
            cg_expr(expr->tuple_expressions[i]);
    }
    
    void AFunction::cg_arrayexpr(Parser::ArrayLiteralExprNode* expr)
    {
        Typechecker::ArrayRType* array_r_type = static_cast<Typechecker::ArrayRType*>(expr->resolvedType.get());
        unsigned int element_size = calc_stack_size(array_r_type->element_type);
        unsigned int heap_size = element_size * expr->array_expressions.size();

        // Check for overflow
        if (element_size != 0 && heap_size / element_size != expr->array_expressions.size())
            throw CompilerException("Array literal was too big to store.");
        
        for (int i = expr->array_expressions.size() - 1 ; i >= 0; i--)
            cg_expr(expr->array_expressions[i]);

        assembly_code.push_back("mov  rdi, " + std::to_string(heap_size));
        FUNCTION_CALL_ALIGNMENT_CHECK(0)
        assembly_code.push_back("call _jpl_alloc");
        FUNCTION_CALL_ALIGNMENT_CLOSE

        assembly_code.push_back("; moving " + std::to_string(heap_size) + " from rsp to rax onto the heap.");
        
        for (int i = heap_size / 8 - 1; i >= 0; i-- )
        {
            unsigned int offset = i * 8; 
            assembly_code.push_back("mov r10, [rsp + " + std::to_string(offset) + "]");
            assembly_code.push_back("mov [rax + " + std::to_string(offset) + "], r10");
        }

        assembly_code.push_back("add rsp, " + std::to_string(heap_size));
        stack_size -= heap_size;
        assembly_code.push_back("push rax");
        stack_size += 8;
        assembly_code.push_back("mov rax, " + std::to_string(expr->array_expressions.size()));
        assembly_code.push_back("push rax");
        stack_size += 8;
    }
    
#pragma endregion

#pragma region Tuple and Array Accesses

    void AFunction::cg_tupleaccessexpr(Parser::TupleIndexExprNode* expr)
    {
        cg_expr(expr->tuple_expression);

        long tuple_index = expr->tuple_index;

        unsigned int total_tuple_size = calc_stack_size(expr->tuple_expression->resolvedType);
        Typechecker::TupleRType* tuple_r_type = static_cast<Typechecker::TupleRType*>(expr->tuple_expression->resolvedType.get());
        unsigned int element_size = calc_stack_size(tuple_r_type->element_types[tuple_index]);
        
        unsigned int move_operations = element_size / 8;
        unsigned int element_offset = 0;
        for (int i = 0; i < tuple_index; i++)
            element_offset += calc_stack_size(tuple_r_type->element_types[i]);
        unsigned int stack_size_removed = total_tuple_size - element_size;
        
        assembly_code.push_back("; moving " + std::to_string(element_size) + " bytes from rsp  + " + std::to_string(element_offset) + " to rsp + " + std::to_string(stack_size_removed));

        for (int i = move_operations - 1; i >= 0; i--)
        {
            unsigned int initial_offset = element_offset + i * 8;
            unsigned int final_offset = stack_size_removed + i * 8;
            assembly_code.push_back("mov r10, [rsp " + ((initial_offset == 0) ? "" : "+ " + std::to_string(initial_offset)) + "]");
            assembly_code.push_back("mov [rsp " + ((final_offset == 0) ? "" : "+ " + std::to_string(final_offset)) + "], r10");
        }

        assembly_code.push_back("add rsp, " + std::to_string(stack_size_removed));
        stack_size -= stack_size_removed;
    }

#pragma endregion

    void AFunction::cg_variableexpr(Parser::VariableExprNode* expr)
    {
        if (assembly.get_optimization_level() > 1 && expr->cp->type == Parser::CPValue::INT)
        {
            Parser::IntValue* int_val = static_cast<Parser::IntValue*>(expr->cp.get());

            if (under_32_bits(int_val->value))
            {
                cg_push_constant_int(int_val->value, expr->token_s);
                return;
            }
        }

        if (stack_size.has_temporary(expr->token_s))
        {
            int offset = stack_size.get_offset(expr->token_s);
            unsigned int bytes_to_move = calc_stack_size(expr->resolvedType);
            assembly_code.push_back("sub rsp, " + std::to_string(bytes_to_move));
            stack_size += bytes_to_move;
            assembly_code.push_back("; Moving " + std::to_string(bytes_to_move) + " bytes from rbp - " + std::to_string(offset) + " to rsp for temp " + expr->token_s);
            for (int i = bytes_to_move - 8; i >= 0; i -= 8)
            {
                assembly_code.push_back("mov r10, [rbp - " + std::to_string(offset) + " + " + std::to_string(i) + "]");
                assembly_code.push_back("mov [rsp + " + std::to_string(i) +"], r10");
            }
        }
        else
        {
            int offset = global_stack->get_offset(expr->token_s);
            unsigned int bytes_to_move = calc_stack_size(expr->resolvedType);
            assembly_code.push_back("sub rsp, " + std::to_string(bytes_to_move));
            stack_size += bytes_to_move;
            assembly_code.push_back("; Moving " + std::to_string(bytes_to_move) + " bytes from rbp - " + std::to_string(offset) + " to rsp for temp " + expr->token_s);
            for (int i = bytes_to_move - 8; i >= 0; i -= 8)
            {
                assembly_code.push_back("mov r10, [r12 - " + std::to_string(offset) + " + " + std::to_string(i) + "]");
                assembly_code.push_back("mov [rsp + " + std::to_string(i) +"], r10");
            }
        }
    }

    void AFunction::cg_callexpr(Parser::CallExprNode* expr)
    {
        CallingConvention cc = assembly.get_calling_convention(expr->function_name);

        if (! cc.is_void_return && cc.return_location == CallingConvention::STACK)
        {
            // Add space on stack for return and save return address in a temp var (not in rdi).
            assembly_code.push_back("sub rsp, " + std::to_string(cc.return_size) + " ; Allocating space for return ");
            stack_size += cc.return_size;
        }

        FUNCTION_CALL_ALIGNMENT_CHECK(cc.stack_argument_size);

        for (int order_index = cc.argument_pop_order.size() - 1; order_index >= 0; order_index--)
        {
            int argument_index = cc.argument_pop_order[order_index].argument_number;
            cg_expr(expr->arguments[argument_index]);
            // calculate the value of the expression
        }

        for (const CallingConvention::MemoryLocationData& memdata : cc.argument_pop_order)
        {
            if (CallingConvention::is_r_register(memdata.location))
            {
                assembly_code.push_back("pop " + CallingConvention::get_register_name(memdata.location));
                stack_size -= 8;
            }
            else if (CallingConvention::is_f_register(memdata.location))
            {
                assembly_code.push_back("movsd " + CallingConvention::get_register_name(memdata.location) + ", [rsp]");
                assembly_code.push_back("add rsp, 8");
                stack_size -= 8;
            }
            else
                break;
        }

        if (! cc.is_void_return && cc.return_location == CallingConvention::STACK)
        {
            unsigned int distance_from_return = cc.stack_argument_size + ((needs_alignment)? 8 : 0);
            assembly_code.push_back("lea rdi, [rsp + " + std::to_string(distance_from_return) + "]; putting return into rdi");
        }

        assembly_code.push_back("call _" + expr->function_name);
        
        for (int i = 0; i < cc.argument_pop_order.size(); i++)
        {
            const CallingConvention::MemoryLocationData& data = cc.argument_pop_order[i];
            if (data.location == CallingConvention::STACK)
            {
                unsigned int bytes_to_remove = calc_stack_size(cc.arg_signature[data.argument_number]);
                assembly_code.push_back("add rsp, " + std::to_string(bytes_to_remove));
                stack_size -= bytes_to_remove;
            }
        }

        /*
        if (cc.stack_argument_size > 0)
        {
            The GOOD way to remove all the stack arguments
            assembly_code.push_back("add rsp, " + std::to_string(cc.stack_argument_size));
            stack_size -= cc.stack_argument_size;
        }*/
        
        FUNCTION_CALL_ALIGNMENT_CLOSE

        if (! cc.is_void_return)
        {
            if (CallingConvention::is_r_register(cc.return_location))
            {
                assembly_code.push_back("push " + CallingConvention::get_register_name(cc.return_location));
                stack_size += 8;
            }

            if (CallingConvention::is_f_register(cc.return_location))
            {
                assembly_code.push_back("sub rsp, 8");
                assembly_code.push_back("movsd [rsp], " + CallingConvention::get_register_name(cc.return_location));
                stack_size += 8;
            }
        }
    }

    void AFunction::cg_ifexpr(Parser::IfExprNode* expr)
    {
        cg_expr(expr->condition);
        
        // if b then 1 else 0 optimization
        if (assembly.get_optimization_level() == 1)
        {
            Parser::ExprNode* _then = expr->then_expr.get();
            Parser::ExprNode* _else = expr->else_expr.get();
            Parser::IntExprNode* then_cast;
            Parser::IntExprNode* else_cast;
            bool then_cast_success = tryCastExpr<Parser::IntExprNode>(_then, then_cast);
            bool else_cast_success = tryCastExpr<Parser::IntExprNode>(_else, else_cast);
            if (then_cast_success && else_cast_success && then_cast->value == 1 && else_cast->value == 0)
                return;
        }
        else if (assembly.get_optimization_level() > 1)
        {
            Parser::CPValue* _then = expr->then_expr->cp.get();
            Parser::CPValue* _else = expr->else_expr->cp.get();
            bool then_cast_success = _then->type == Parser::CPValue::INT;
            bool else_cast_success = _else->type == Parser::CPValue::INT;
            if (then_cast_success && else_cast_success)
            {
                Parser::IntValue* then_cast = static_cast<Parser::IntValue*>(_then);
                Parser::IntValue* else_cast = static_cast<Parser::IntValue*>(_else);
                
                if (then_cast->value == 1 && else_cast->value == 0)
                    return;
            }
        }

        // regular if statement
        assembly_code.push_back("pop rax");
        stack_size -= 8;
        assembly_code.push_back("cmp rax, 0 ; " + expr->token_s);

        std::string else_jump = assembly.get_new_jump();
        std::string end_jump = assembly.get_new_jump();

        assembly_code.push_back("je " + else_jump);
        // Then
        cg_expr(expr->then_expr);
        assembly_code.push_back("jmp " + end_jump);
        
        stack_size -= calc_stack_size(expr->resolvedType); // Only one of the two options will get pushed.

        // Else
        assembly_code.push_back(else_jump + ":");
        cg_expr(expr->else_expr);

        assembly_code.push_back(end_jump + ":");
    }

    inline void AFunction::cg_shortcircuit(Parser::BinopExprNode* expr)
    {
        assembly_code.push_back("; " + expr->token_s);

        std::string jmp = "";
        
        switch (expr->operation)
        {
        case Parser::BinopExprNode::AND:
            jmp = "je ";
            break;
        case Parser::BinopExprNode::OR:
            jmp = "jne ";
            break;
        default:
            throw CompilerException("Unrecognized short-circuit operation " + expr->token_s + ".");
        }

        cg_expr(expr->lhs);
        
        assembly_code.push_back("pop rax");
        stack_size -= 8;
        assembly_code.push_back("cmp rax, 0");
        std::string rhs_skip_label = assembly.get_new_jump();
        assembly_code.push_back(jmp + rhs_skip_label);
        
        cg_expr(expr->rhs);
        assembly_code.push_back("pop rax");
        stack_size -= 8;
        
        assembly_code.push_back(rhs_skip_label + ":");
        assembly_code.push_back("push rax");
        stack_size += 8;
    }

#pragma endregion

#pragma region Statements

    bool AFunction::cg_stmt(Parser::StmtNode* stmt, CallingConvention cc)
    {
        {
            Parser::LetStmtNode* result;
            if (tryCastStmt<Parser::LetStmtNode>(stmt, result))
            {
                cg_letstmt(result);
                return false;
            }
        }

        {
            Parser::ReturnStmtNode* result;
            if (tryCastStmt<Parser::ReturnStmtNode>(stmt, result))
            {
                cg_returnstmt(result, cc);
                return true;
            }
        }

        {
            Parser::AssertStmtNode* result;
            if (tryCastStmt<Parser::AssertStmtNode>(stmt, result))
            {
                cg_assertstmt(result);
                return false;
            }
        }

        throw CompilerException("Could not generate code for unimplemented statement.");
    }

    void AFunction::cg_letstmt(Parser::LetStmtNode* stmt)
    {
        cg_expr(stmt->variable_expression);
        assembly_code.push_back("; " + stmt->token_s + " | line: " + std::to_string(stmt->line));
        stack_size.add_lvalue(stmt->set_variable_name.get(), stmt->variable_expression->resolvedType, stack_size.get_size_of_temporaries());
    }

    void AFunction::cg_returnstmt(Parser::ReturnStmtNode* stmt, CallingConvention cc)
    {
        cg_expr(stmt->expression);

        add_function_return_code(cc);
    }

    void AFunction::cg_arrayindexexpr(Parser::ArrayIndexExprNode* expr)
    {
        Parser::ExprNode* array_non_cast = expr->array_expression.get();
        Parser::VariableExprNode* array_cast;
        bool  optimize_array_copy = (assembly.get_optimization_level() > 0) && (tryCastExpr<Parser::VariableExprNode>(array_non_cast, array_cast));

        if (! optimize_array_copy)
            cg_expr(expr->array_expression);
        
        // Push indices in reverse order
        for (int i  = expr->array_indices.size() - 1; i >= 0; i--)
        {
            cg_expr(expr->array_indices[i]);
        }

        // Check indices are valid
        long indices_size = expr->array_indices.size() * 8;
        long gap = (optimize_array_copy) ? stack_size.get_stack_size() - stack_size.get_offset(array_cast->token_s) : indices_size;

        std::string neg_expt = "negative array index";
        std::string neg_expt_const = assembly.add_constant_string(neg_expt);
        std::string ovr_expt = "index too large";
        std::string ovr_expt_const = assembly.add_constant_string(ovr_expt);
        
        for (int i  = 0; i < expr->array_indices.size(); i++)
        {
            std::string neg_good_jump = assembly.get_new_jump();
            std::string ovr_good_jump = assembly.get_new_jump();

            // negative
            assembly_code.push_back("mov rax, [rsp + " + std::to_string(i * 8) + "]");
            assembly_code.push_back("cmp rax, 0");
            assembly_code.push_back("jge " + neg_good_jump);
            {
            FUNCTION_CALL_ALIGNMENT_CHECK(0);
            assembly_code.push_back("lea rdi, [rel " + neg_expt_const + "] ; " + neg_expt);
            assembly_code.push_back("call _fail_assertion");
            FUNCTION_CALL_ALIGNMENT_CLOSE
            }
            assembly_code.push_back(neg_good_jump + ":");

            // overflow
            assembly_code.push_back("cmp rax, [rsp + " + std::to_string(i * 8 + gap) + "]");
            assembly_code.push_back("jl " + ovr_good_jump);
            {
            FUNCTION_CALL_ALIGNMENT_CHECK(0);
            assembly_code.push_back("lea rdi, [rel " + ovr_expt_const + "] ; " + ovr_expt);
            assembly_code.push_back("call _fail_assertion");
            FUNCTION_CALL_ALIGNMENT_CLOSE
            }
            assembly_code.push_back(ovr_good_jump + ":");
        }

        // Compute address to index into
        if (assembly.get_optimization_level() < 1)
        {  // mov rax, 0 ; imul rax, [rsp + ... ] ; add rax, [rsp + ...] is wasteful
            assembly_code.push_back("mov rax, 0");

            for (int i = 0; i < expr->array_indices.size(); i++)
            {
                assembly_code.push_back("imul rax, [rsp + " + std::to_string(i * 8 + gap)  + "]");
                assembly_code.push_back("add rax, [rsp + " + std::to_string(i * 8) + "]");
            }
        }
        else if (assembly.get_optimization_level() == 1 || expr->array_expression->cp->type != Parser::CPValue::ARRAY)
        { // mov rax, [rsp]
            assembly_code.push_back("mov rax, [rsp]");

            for (int i = 1; i < expr->array_indices.size(); i++)
            { // optimize ?
                assembly_code.push_back("imul rax, [rsp + " + std::to_string(i * 8 + gap)  + "]");
                assembly_code.push_back("add rax, [rsp + " + std::to_string(i * 8) + "]");
            }
        }
        else
        {
            assembly_code.push_back("mov rax, [rsp]");
            Parser::ArrayValue* array_value = static_cast<Parser::ArrayValue*>(expr->array_expression->cp.get());

            for (int i = 1; i < expr->array_indices.size(); i++)
            { // optimize ?
                Parser::CPValue* index_value = array_value->lengths[i].get();
                
                if (index_value->type == Parser::CPValue::INT)
                {
                    Parser::IntValue* int_index_value = static_cast<Parser::IntValue*>(index_value);
                    long mult_amount = int_index_value->value;
                    long power;
                    if (is_power_of_two(mult_amount, power))
                        assembly_code.push_back("shl rax, " + std::to_string(power));
                    else        
                        assembly_code.push_back("imul rax, " + std::to_string(mult_amount));
                }
                else
                    assembly_code.push_back("imul rax, [rsp + " + std::to_string(i * 8 + gap)  + "]");
                assembly_code.push_back("add rax, [rsp + " + std::to_string(i * 8) + "]");
            }
        }
        // optimize
        long mult_amount = calc_stack_size(expr->resolvedType);
        long power;
        if (assembly.get_optimization_level() > 0 && is_power_of_two(mult_amount, power))
            assembly_code.push_back("shl rax, " + std::to_string(power) + " ; multiply by size of elements");
        else        
            assembly_code.push_back("imul rax, " + std::to_string(mult_amount) + " ; multiply by size of elements");
        assembly_code.push_back("add rax, [rsp + " + std::to_string(indices_size + gap) + "] ; add ptr for address in heap");

        // Free indices
        if (! optimize_array_copy)
        {
            for (int i = 0; i < expr->array_indices.size(); i++)
            {
                assembly_code.push_back("add rsp, 8");
                stack_size -= 8;
            }
        }
        else // THE METHOD FOR FREEING INDICES CHANGED
        {
            assembly_code.push_back("add rsp, " + std::to_string(indices_size));
            stack_size -= indices_size;
        }

        // Free array
        if (! optimize_array_copy)
        {
            assembly_code.push_back("add rsp, " + std::to_string(calc_stack_size(expr->array_expression->resolvedType)));
            stack_size -= calc_stack_size(expr->array_expression->resolvedType);
        }

        // Get value off the stack
        assembly_code.push_back("sub rsp, " + std::to_string(calc_stack_size(expr->resolvedType)));
        stack_size += calc_stack_size(expr->resolvedType);

        unsigned int bytes_to_move = calc_stack_size(expr->resolvedType);

        assembly_code.push_back("; Extracting array element of " + std::to_string(bytes_to_move) + " bytes from rax to rsp");
        move_bytes(bytes_to_move, "rax", "rsp");
    }

    void AFunction::cg_loopexpr(Parser::LoopExprNode* expr)
    {
        bool sum_is_int = true;
        
        [[maybe_unused]] Parser::SumLoopExprNode* _;
        bool is_sum = tryCast<Parser::LoopExprNode, Parser::SumLoopExprNode>(expr, _);
        // TODO: support array loop. Assuming sum loop for now
        
        // Make room for counter
        if (is_sum)
        {
            assembly_code.push_back("sub rsp, 8 ; 8 bytes for sum");
            stack_size += 8;
            sum_is_int = expr->resolvedType->type_name == Typechecker::INT;
        }
        else
        {
            // set up array on heap
            assembly_code.push_back("sub rsp, 8 ; 8 bytes for array ptr");
            stack_size += 8;
        }

        // Compute loop bounds (check gt zero)
        std::string invalid_bound_expt = "non-positive loop bound";
        for (int i = expr->bounds.size() - 1; i >= 0; i--)
        {
            assembly_code.push_back("; Adding " + expr->bounds[i]->first + " bound to stack.");
            cg_expr(expr->bounds[i]->second);

            std::string valid_jump = assembly.get_new_jump();
            assembly_code.push_back("mov rax, [rsp]");
            assembly_code.push_back("cmp rax, 0");
            assembly_code.push_back("jg " + valid_jump);
            FUNCTION_CALL_ALIGNMENT_CHECK(0)
            std::string invalid_bound_expt_const = assembly.add_constant_string(invalid_bound_expt);
            assembly_code.push_back("lea rdi, [rel " + invalid_bound_expt_const + "]");    
            assembly_code.push_back("call _fail_assertion");
            FUNCTION_CALL_ALIGNMENT_CLOSE
            assembly_code.push_back(valid_jump + ":");
        }

        int indices_size = expr->bounds.size() * 8;

        // Write 0 to counter loc
        if (is_sum)
        {
            assembly_code.push_back("mov rax, 0");
            assembly_code.push_back("mov [rsp + " + std::to_string(expr->bounds.size() * 8) + "], rax ; initialize sum");
        }
        else // Allocate an array
        {
            unsigned int element_size = calc_stack_size(expr->loop_expression->resolvedType);
            
            // calc size
            assembly_code.push_back("; Computing total size of heap memory to allocate.");
            assembly_code.push_back("mov rdi, " + std::to_string(element_size) + " ; sizeof array element");
            
            std::string ovr_expt = "overflow computing array size";
            for (int i = 0; i < expr->bounds.size(); i++)
            {
                std::string no_ovr_jump = assembly.get_new_jump();
                std::string ovr_expt_const = assembly.add_constant_string(ovr_expt);
                // don't optimize. Need check for overflow
                assembly_code.push_back("imul rdi, [rsp + " + std::to_string(i * 8) + "] ; multiply by " + expr->bounds[i]->second->token_s);
                // check for overflow
                assembly_code.push_back("jno " + no_ovr_jump + " ; check that " + expr->bounds[i]->first + "'s bound doesn't overflow");
                FUNCTION_CALL_ALIGNMENT_CHECK(0)
                assembly_code.push_back("lea rdi, [rel " + ovr_expt_const + "] ; " + ovr_expt);
                assembly_code.push_back("call _fail_assertion");
                FUNCTION_CALL_ALIGNMENT_CLOSE
                assembly_code.push_back(no_ovr_jump + ":");
            }
            
            // allocate array
            FUNCTION_CALL_ALIGNMENT_CHECK(0)
            assembly_code.push_back("call _jpl_alloc ; allocate array");
            FUNCTION_CALL_ALIGNMENT_CLOSE
            assembly_code.push_back("mov [rsp + " + std::to_string(indices_size) + "], rax ; Move array pointer to stack");
        }

        // Push indices (default value 0; save where on the stack it is)
        for (int i = expr->bounds.size() - 1; i >= 0; i--)
        {
            assembly_code.push_back("mov rax, 0");
            assembly_code.push_back("push rax; adding " + expr->bounds[i]->first + " to stack.");
            stack_size += 8;
            stack_size.add_temporary(expr->bounds[i]->first, stack_size.get_size_of_temporaries());
        }

        // Loop body (label + compute + add to counter)
        std::string loop_body_jump = assembly.get_new_jump();

        assembly_code.push_back(loop_body_jump + ": ; loop body");
        cg_expr(expr->loop_expression);

        if (is_sum)
        {
            if (sum_is_int)
            {
                assembly_code.push_back("pop rax");
                stack_size -= 8;
                assembly_code.push_back("add [rsp + " + std::to_string(indices_size * 2) + "], rax ; Add loop body to sum");
            }
            else
            {
                assembly_code.push_back("movsd xmm0, [rsp]");
                assembly_code.push_back("add rsp, 8");
                stack_size -= 8;
                assembly_code.push_back("addsd xmm0, [rsp + " + std::to_string(indices_size * 2) + "] ; Load sum");
                assembly_code.push_back("movsd [rsp + " + std::to_string(indices_size * 2) + "], xmm0 ; Save sum");
            }
        }
        else // Update array on heap
        {
            unsigned int element_size = calc_stack_size(expr->loop_expression->resolvedType);
            
            // Calculate storage index
            if (assembly.get_optimization_level() < 1)
            {
                assembly_code.push_back("mov rax, 0");

                for (int i = 0; i < expr->bounds.size(); i++)
                {
                    assembly_code.push_back("imul rax, [rsp + " + std::to_string(element_size + i * 8 + indices_size)  + "]");
                    assembly_code.push_back("add rax, [rsp + " + std::to_string(element_size + i * 8) + "]");
                }
            }
            else
            {
                assembly_code.push_back("mov rax, [rsp + " + std::to_string(element_size) + "]");

                for (int i = 1; i < expr->bounds.size(); i++)
                {
                    Parser::ExprNode* bound = expr->bounds[i]->second.get();
                    bool is_constant = false;
                    long constant_value;

                    if (assembly.get_optimization_level() == 1)
                    {
                        Parser::IntExprNode* bound_constant;
                        is_constant = tryCastExpr<Parser::IntExprNode>(bound, bound_constant);
                        constant_value = bound_constant->value;
                    }
                    else // work with CPValues
                    {
                        is_constant = bound->cp->type == Parser::CPValue::INT;
                        if (is_constant)
                            constant_value = static_cast<Parser::IntValue*>(bound->cp.get())->value;
                    }
                    

                    if (is_constant)
                    { // optimize
                        long power;
                        if (is_power_of_two(constant_value, power))
                            assembly_code.push_back("shl rax, " + std::to_string(power));
                        else if (under_32_bits(constant_value))
                            assembly_code.push_back("imul rax, " + std::to_string(constant_value));
                        else
                            assembly_code.push_back("imul rax, [rsp + " + std::to_string(element_size + i * 8 + indices_size)  + "]");
                    }
                    else
                        assembly_code.push_back("imul rax, [rsp + " + std::to_string(element_size + i * 8 + indices_size)  + "]");
    
                    assembly_code.push_back("add rax, [rsp + " + std::to_string(element_size + i * 8) + "]");
                }
            }

            // optimize
            long power;
            if (assembly.get_optimization_level() > 0 && is_power_of_two(element_size, power))
                assembly_code.push_back("shl rax, " + std::to_string(power) + " ; multiply by size of elements");
            else
                assembly_code.push_back("imul rax, " + std::to_string(element_size) + " ; multiply by size of elements");

            assembly_code.push_back("add rax, [rsp + " + std::to_string(element_size + indices_size * 2) + "] ; add ptr for address in heap");

            // Move element
            assembly_code.push_back("; Moving newly created element into array");
            move_bytes(element_size, "rsp", "rax");

            assembly_code.push_back("add rsp, " + std::to_string(element_size));
            stack_size -= element_size;
        }

        // Increment indices (and if overflow, increment next)
        for (int i = expr->bounds.size() - 1; i >= 0; i--)
        {
            std::string index_name = expr->bounds[i]->first;

            assembly_code.push_back("; Increment " + index_name);
            assembly_code.push_back("add qword [rsp + " + std::to_string(i * 8) + "], 1");
            assembly_code.push_back("mov rax, [rsp + " + std::to_string(i * 8) + "]");
            assembly_code.push_back("cmp rax, [rsp + " + std::to_string(i * 8 + indices_size) +"]");
            assembly_code.push_back("jl " + loop_body_jump + " ; If "+ index_name +" < bound, next iter");
            if (i != 0)
                assembly_code.push_back("mov qword [rsp + " + std::to_string(i * 8) + "], 0 ; "+ index_name +" = 0");
        }

        // Free loop indices and bounds (keep counter or pointer)
        assembly_code.push_back("; end loop body");
        assembly_code.push_back("add rsp, " + std::to_string(indices_size) + " ; free loop indices");
        stack_size -= indices_size;
        if (is_sum) // If we're making an array, the bounds are part of the array and should not be removed.
        {
            assembly_code.push_back("add rsp, " + std::to_string(indices_size) + " ; free loop bounds");
            stack_size -= indices_size;
        }
    }

    void AFunction::cg_assertstmt(Parser::AssertStmtNode* stmt)
    {
        cg_expr(stmt->expression);
        assembly_code.push_back("pop rax");
        stack_size -= 8;
        assembly_code.push_back("cmp rax, 0 ; check assert");
        std::string jump_name = assembly.get_new_jump();
        assembly_code.push_back("jne " + jump_name);
        FUNCTION_CALL_ALIGNMENT_CHECK(0)
        std::string error_message_constant = assembly.add_constant_string(stmt->string->getValue());
        assembly_code.push_back("lea rdi, [rel " + error_message_constant + "] ; " + stmt->string->getValue());
        assembly_code.push_back("call _fail_assertion");
        FUNCTION_CALL_ALIGNMENT_CLOSE
        assembly_code.push_back(jump_name + ":");
    }

    void AFunction::add_function_return_code(CallingConvention cc)
    {
        // Set up Return
        if (! cc.is_void_return)
        {
            if (cc.return_location == CallingConvention::RAX)
            {
                assembly_code.push_back("pop rax");
                stack_size -= 8;
            }
            else if (cc.return_location == CallingConvention::XMM0)
            {
                assembly_code.push_back("movsd xmm0, [rsp]");
                assembly_code.push_back("add rsp, 8");
                stack_size -= 8;
            }
            else
            {
                assembly_code.push_back("mov rax, [rbp - " + std::to_string(stack_size.get_offset("$return")) +"] ; Address to write return value into");
                
                unsigned int bytes_to_move = cc.return_size;
                
                assembly_code.push_back("; Moving " + std::to_string(bytes_to_move) +  " bytes from rsp to rax");
                for (int i = bytes_to_move - 8; i >= 0; i-= 8)
                {
                    assembly_code.push_back("mov r10, [rsp + " + std::to_string(i) + "]");
                    assembly_code.push_back("mov [rax + " + std::to_string(i) + "], r10");
                }
            }
        }


        assembly_code.push_back(";Remove temporary variables");
        assembly_code.push_back("add rsp, " + std::to_string(stack_size.get_size_of_temporaries()) + "\n");

        assembly_code.push_back("; Function Return");
        assembly_code.push_back("pop rbp");
        assembly_code.push_back("ret");
    }
    void AFunction::move_bytes(unsigned int bytes_to_move, std::string from, std::string to)
    {
        for (int i = bytes_to_move - 8; i >= 0; i -= 8)
        {
            assembly_code.push_back("mov r10, [" + from + " + " + std::to_string(i) + "]");
            assembly_code.push_back("mov [" + to + " + " + std::to_string(i) + "], r10");
        }
    }


#pragma endregion

#pragma region Optimizations

    void AFunction::cg_push_constant_int(long constant_value, std::string extra_comments)
    {
        bool is_constant_under_32_bits = under_32_bits(constant_value);
        
        if (assembly.get_optimization_level() > 0 && is_constant_under_32_bits)
        {
            assembly_code.push_back("push qword " + std::to_string(constant_value) + " ; " + extra_comments);
        }
        else
        {
            std::string const_name = assembly.add_constant_int(constant_value);
            assembly_code.push_back("mov rax, [rel "+ const_name +"] ; " + std::to_string(constant_value) + " " + extra_comments);
            assembly_code.push_back("push rax");
        }
        stack_size.increment_stack_size(8);
    }

#pragma endregion

    bool AFunction::is_power_of_two(long to_check, long& power)
    {
        if (to_check >= 0 && (to_check & (to_check - 1)) == 0)
        {
            for (power = 0; (1l << power) < to_check; power++);

            return true;
        }

        return false;
    }


    std::string AFunction::toString()
    {
        std::string code = name + ":\n_" + name + ":\n";
        
        code += "; Function Stack Setup\n\tpush rbp\n\tmov rbp, rsp\n";

        if (is_main)
            code += "\n; Setting Up r12\n\tpush r12\n\tmov r12, rbp\n";

        for (std::string assembly_line : assembly_code)
        {
            std::string head;
            switch (assembly_line[0])
            {
            case ';':
                head = "\n";
                break;
            case '.':
                head = "";
                break;
            default:
                head = "\t";
                break;
            }
            code += head + assembly_line + "\n";
        }

        if (is_main)
        {
            if (stack_size.get_size_of_temporaries() != 0)
                code += "\n;Remove temporary variables\n\tadd rsp, " + std::to_string(stack_size.get_size_of_temporaries()) + "\n";

            code += "\n; Restore r12\n\tpop r12\n";

            code += "\n; Function Return\n\tpop rbp\n\tret\n";
        }
        return code;
    }

#pragma endregion

}