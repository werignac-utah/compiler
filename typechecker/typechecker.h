#include "types.h"
#include "../parser/parser.h"
#include <unordered_map>

#ifndef __TYPECHECKER_H__
#define __TYPECHECKER_H__

namespace Typechecker
{
    class TypeException : public std::exception 
    {
        public:
            std::string message;
            TypeException(const std::string& m, const Parser::ASTNode* node);
            const char* what () const noexcept override;
    };

    struct NameInfo
    {
        public:
            virtual ~NameInfo() {}
    };

    struct VariableInfo : public NameInfo
    {
        public:
            std::shared_ptr<ResolvedType> rtype;
            VariableInfo(std::shared_ptr<ResolvedType>& _rtype) : rtype(_rtype) {};
            virtual ~VariableInfo() {}
    };

    struct TypeInfo : public NameInfo
    {
        public:
            std::shared_ptr<ResolvedType> stored_type;
            TypeInfo(std::shared_ptr<ResolvedType> _stored_type) : stored_type(_stored_type) {};
            virtual ~TypeInfo() {}
    };

    struct FuncInfo : public NameInfo
    {
        public:
            std::shared_ptr<ResolvedType> return_type;
            std::vector<std::shared_ptr<ResolvedType>> arguments;
            FuncInfo(std::shared_ptr<ResolvedType> _return_type, std::vector<std::shared_ptr<ResolvedType>>& _arguments) : return_type(_return_type), arguments(_arguments) {};
            virtual ~FuncInfo() {};
    };

    class Scope
    {
        private:
            Scope* parent;
        public:
            std::unordered_map<std::string, std::unique_ptr<NameInfo>> symbol_table;

        public:
            // Constructs a root (global) scope. 
            Scope(){parent = nullptr;}
            // Constructs a new child scope and returns it.
            std::shared_ptr<Scope> createNestedScope();
            // Adds the given name to this scope's symbol table.
            // If the name already exists in this scope or a parent, returns false.
            bool add(std::string name, NameInfo* info);
            void add_argument(Parser::ArgumentNode* argument, std::shared_ptr<ResolvedType>& rtype);
            void add_lvalue(Parser::LValue* lvalue, std::shared_ptr<ResolvedType>& rtype);
            // Checks if the given name exists in this scope or a parent.
            // Returns whether the lookup was successful. Fills out the info pointer
            // if so.
            bool lookup(std::string name, NameInfo*& info);
            NameInfo* get_info(std::string name);
    };

    std::shared_ptr<Scope> create_global_scope();
    // Used in bindings
    std::shared_ptr<ResolvedType> resolve_type(std::unique_ptr<Parser::TypeNode>& u_type, std::shared_ptr<Scope>& scope);
    
    class PseudoLValue : public Parser::LValue
    {
        public:
            PseudoLValue(Parser::ASTNode* _replacement);
            Parser::ASTNode* replacement;
            virtual ~PseudoLValue() {};
            virtual std::string toString() {return "( ~PseudoLValue " + replacement->toString() + " )";}
    };

    class PseudoArgumentLValue : public PseudoLValue
    {
        public:
            PseudoArgumentLValue(Parser::ArgumentNode* _arg, Parser::ASTNode* replacement) : PseudoLValue(replacement), argument(_arg) {};
            virtual ~PseudoArgumentLValue() {};
            Parser::ArgumentNode* argument;
    };

    class PseudoTupleLValue : public PseudoLValue
    {
        public:
            PseudoTupleLValue(std::vector<PseudoLValue*> _tuple, Parser::ASTNode* replacement);
            virtual ~PseudoTupleLValue() {};
            std::vector<std::unique_ptr<PseudoLValue>> lvalues;
    };
    
    std::pair<PseudoLValue*, std::shared_ptr<ResolvedType>> decompose_binding(std::unique_ptr<Parser::BindingNode>& u_binding, std::shared_ptr<Scope>& scope);
    // Get an expression and return its resolved type based on the
    // expression type (arithmetic, array, etc.).
    std::shared_ptr<ResolvedType> type_of(std::unique_ptr<Parser::ExprNode>& expr, std::shared_ptr<Scope>& scope);
    // Get a list of commands and ensure that the types of the command
    // expressions make sense. Also, assign the expression nodes resolved
    // types.
    std::shared_ptr<Scope> typecheck(std::vector<std::unique_ptr<Parser::CmdNode>>& commands);

    void typecheckCmd(std::unique_ptr<Parser::CmdNode>& u_cmd, std::shared_ptr<Scope>& scope);
    // Returns true if this stmt was a return statement.
    bool typecheckStmt(std::unique_ptr<Parser::StmtNode>& u_stmt, std::shared_ptr<Scope>& scope, const std::shared_ptr<ResolvedType>& return_type);
}//namespace Typechecker

#endif

