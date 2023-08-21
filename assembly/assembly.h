#include <vector>
#include <utility>
#include <memory>
#include <unordered_map>
#include "../parser/parser.h"
#include "../typechecker/typechecker.h"
#include "../typechecker/types.h"

#ifndef __ASSEMBLY_H__
#define __ASSEMBLY_H__

namespace Compiler
{
    class CompilerException : public std::exception
    {
        public:
            std::string message;
            CompilerException(const std::string& m);
            const char* what() const noexcept override;
    };

    class IFunction
    {
    public:
        virtual std::string toString() = 0;
        virtual ~IFunction() {};
    };
    
    const std::string linkage_header = "global jpl_main\n"
            "global _jpl_main\n"
            "extern _fail_assertion\n"
            "extern _jpl_alloc\n"
            "extern _get_time\n"
            "extern _show\n"
            "extern _print\n"
            "extern _print_time\n"
            "extern _read_image\n"
            "extern _write_image\n"
            "extern _fmod\n"
            "extern _sqrt\n"
            "extern _exp\n"
            "extern _sin\n"
            "extern _cos\n"
            "extern _tan\n"
            "extern _asin\n"
            "extern _acos\n"
            "extern _atan\n"
            "extern _log\n"
            "extern _pow\n"
            "extern _atan2\n"
            "extern _to_int\n"
            "extern _to_float\n";

    unsigned int calc_stack_size(std::shared_ptr<Typechecker::ResolvedType> resolved_type);

    class CallingConvention
    {
    public:
        enum MemoryLocation
        {
            RDI = 0, RSI = 1, RDX = 2, RCX = 3, R8 = 4, R9 = 5, XMM0 = 6, XMM1 = 7, XMM2 = 8, XMM3 = 9, XMM4 = 10, XMM5 = 11, XMM6 = 12, XMM7 = 13, STACK = 14, RAX = 15
        };

        typedef struct MemoryLocationData
        {
        public:
            MemoryLocation location;
            int argument_number;
        } MemoryLocationData;

        std::vector<std::shared_ptr<Typechecker::ResolvedType>> arg_signature;
        std::shared_ptr<Typechecker::ResolvedType> ret_signature;

        MemoryLocation return_location;
        bool is_void_return;
        std::vector<MemoryLocationData> argument_pop_order;
        unsigned int stack_argument_size;
        unsigned int return_size;

#define R_REGISTER_COUNT 6
#define F_REGISTER_COUNT 8

        CallingConvention(const std::vector<std::shared_ptr<Typechecker::ResolvedType>>& arguments, const std::shared_ptr<Typechecker::ResolvedType>& return_type);

        static std::string get_register_name(MemoryLocation loc);
        static bool is_r_register(MemoryLocation loc);
        static bool is_f_register(MemoryLocation loc);


    private:
        static bool is_integral(std::shared_ptr<Typechecker::ResolvedType> r_type);
        static bool is_float(std::shared_ptr<Typechecker::ResolvedType> r_type);
        static bool is_agregate(std::shared_ptr<Typechecker::ResolvedType> r_type);
        static bool is_void_return_type(std::shared_ptr<Typechecker::ResolvedType> r_type);
    };

    class Assembly
    {
    private:
        std::vector<std::shared_ptr<IFunction>> functions;
        // Take constants of the form
        // dq 1 or db '(IntType)', 0
        // and return the corresponding name of the constant.
        // const0
        std::vector<std::string> constants;
        unsigned int jump_count = 0;
        std::unordered_map<std::string, CallingConvention> calling_conventions;

        unsigned char optimization_level;

        std::string add_constant_raw(std::string constant);

    public:
        Assembly(const Typechecker::Scope& scope, unsigned char _op_lvl);
        // Adds a coonstant to the list of constants and returns the name.
        // May use a previously defined constant if the contents is the same.
        std::string add_constant_string(std::string string_constant);
        std::string add_constant_int(long int_constant);
        std::string add_constant_float(double float_constant);
        std::string add_constant_true();
        std::string add_constant_false();

        void add_function (std::shared_ptr<IFunction> function);

        std::string get_new_jump();

        unsigned char get_optimization_level() { return optimization_level; }

        std::string toString();

        void add_calling_convention(std::string function_name, CallingConvention convention);
        CallingConvention get_calling_convention(std::string function_name);
    };

    class StackDescription
    {
    private:
        std::unordered_map<std::string, int> temporaries;
        unsigned int stack_size;
        const unsigned int init_stack_size;
    public:
        StackDescription(unsigned int _init_stack_size) : stack_size(_init_stack_size), init_stack_size(_init_stack_size) {} 
        const unsigned int get_stack_size() {return stack_size;}
        const unsigned int increment_stack_size(const unsigned int inc) {return stack_size += inc;}
        const unsigned int decrement_stack_size(const unsigned int dec) {return stack_size -= dec;}
        
        const unsigned int get_size_of_temporaries() {return stack_size - init_stack_size;};

        void add_temporary(std::string name, const int offset);
        void add_argument(Parser::ArgumentNode* argument, const std::shared_ptr<Typechecker::ResolvedType>& r_type, int offset);
        void add_lvalue(Parser::LValue* lvalue, const std::shared_ptr<Typechecker::ResolvedType>& r_type, int offset);
        void add_binding(Parser::BindingNode* binding, const std::shared_ptr<Typechecker::ResolvedType>& r_type, const int offset);

        const unsigned int operator+=(unsigned int inc)
        {
            return increment_stack_size(inc);
        } 
        const unsigned int operator-=(unsigned int dec)
        {
            return decrement_stack_size(dec);
        }

        const int get_offset(std::string temporary_name)
        {
            return temporaries[temporary_name] + init_stack_size;
        }

        bool has_temporary(std::string temporary_name)
        {
            return temporaries.count(temporary_name) > 0;
        }
    };

    class AFunction : public IFunction
    {
    private:
        std::string name;
        Assembly& assembly;
        std::vector<std::string> assembly_code;
        bool is_main;
        StackDescription stack_size;
        StackDescription* global_stack;

    public:
        AFunction(Assembly& _assembly) : name("jpl_main"), assembly(_assembly), is_main(true), stack_size(8), global_stack(&stack_size)
        {
            stack_size.add_temporary("argnum", -24);
            stack_size.add_temporary("args", -24);
        }
        AFunction(Parser::FnCmd* cmd, Assembly& _assembly, StackDescription* _global_stack);
        // Code Generation Methods. Used for writing assembly
        // of any expression or command type. Updates the stack size if necessary.
        void cg_cmd(std::unique_ptr<Parser::CmdNode>& cmd);
        void cg_showcmd(Parser::ShowCmdNode* cmd);
        void cg_letcmd(Parser::LetCmdNode* cmd);

        void cg_readcmd(Parser::ReadCmdNode* cmd);
        void cg_fncmd(Parser::FnCmd* cmd);

        void cg_assertcmd(Parser::AssertCmdNode* cmd);
        void cg_printcmd(Parser::PrintCmdNode* cmd);
        void cg_writecmd(Parser::WriteCmdNode* cmd);
        void cg_timecmd(Parser::TimeCmdNode* cmd);

        void cg_expr(std::unique_ptr<Parser::ExprNode>& expr);
        void cg_intexpr(Parser::IntExprNode* expr);    
        void cg_floatexpr(Parser::FloatExprNode* expr);    
        void cg_trueexpr(Parser::TrueExprNode* expr);    
        void cg_falseexpr(Parser::FalseExprNode* expr);    
        void cg_unopexpr(Parser::UnopExprNode* expr);
        void cg_binopexpr(Parser::BinopExprNode* expr);
        void cg_tupleexpr(Parser::TupleLiteralExprNode* expr);
        void cg_arrayexpr(Parser::ArrayLiteralExprNode* expr);
        void cg_tupleaccessexpr(Parser::TupleIndexExprNode* expr);

        void cg_variableexpr(Parser::VariableExprNode* expr);
        void cg_callexpr(Parser::CallExprNode* expr);

        void cg_ifexpr(Parser::IfExprNode* expr);
        inline void cg_shortcircuit(Parser::BinopExprNode* expr);
        void cg_arrayindexexpr(Parser::ArrayIndexExprNode* expr);
        void cg_loopexpr(Parser::LoopExprNode* expr);

        bool cg_stmt(Parser::StmtNode* stmt, CallingConvention cc);
        void cg_letstmt(Parser::LetStmtNode* stmt);
        void cg_returnstmt(Parser::ReturnStmtNode* stmt, CallingConvention cc);

        void cg_assertstmt(Parser::AssertStmtNode* stmt);

        std::string toString();
        virtual ~AFunction() {};

    private:
        bool under_32_bits(long x) {return (x & ((1l << 31) - 1)) == x;}
        void cg_push_constant_int(long constant_value, std::string extra_comments = "");
        bool is_power_of_two(long to_check, long& power);

    private:
        void add_function_return_code(CallingConvention cc);
        void move_bytes(unsigned int bytes_to_move, std::string from, std::string to);
    };
}

#endif
