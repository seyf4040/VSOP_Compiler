#ifndef CODE_GENERATOR_HPP
#define CODE_GENERATOR_HPP

#include "AST.hpp"
#include "SemanticAnalyzer.hpp"
#include "TypeChecker.hpp"
#include <string>
#include <memory>
#include <map>
#include <stack>
#include <vector>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace VSOP {

class CodeGenerator : public Visitor {
public:
    CodeGenerator(const std::string& source_file, bool output_ir = false);
    ~CodeGenerator();
    
    // Generate code for a program
    bool generate(std::shared_ptr<Program> program);
    
    // Dump LLVM IR to stdout
    void dumpIR();
    
    // Generate executable file
    bool generateExecutable(const std::string& output_file);
    
    // Get error messages
    const std::vector<std::string>& getErrors() const;
    
private:
    // LLVM context and module
    bool using_provided_object = true;
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    
    // VSOP program
    std::shared_ptr<Program> program;
    
    // Semantic analyzer for type information
    SemanticAnalyzer analyzer;
    
    // Output flags
    bool output_ir;
    std::string source_file;
    std::string output_file;
    
    // Error tracking
    std::vector<std::string> errors;
    
    // Current context
    std::string current_class;
    std::string current_method;
    
    // Symbol tables
    struct ClassLayout {
        llvm::StructType* type;                       // LLVM type for class
        llvm::Function* constructor;                  // Constructor function
        std::map<std::string, int> field_indices;     // Field indices in struct
        std::map<std::string, llvm::Function*> methods; // Method functions
        std::string parent;                           // Parent class name
    };
    
    std::map<std::string, ClassLayout> class_layouts;
    std::map<std::string, llvm::AllocaInst*> named_values; // For local variables
    std::stack<llvm::BasicBlock*> loop_exit_blocks;    // For "while" statements
    llvm::AllocaInst* current_self;                  // Current "self" pointer
    
    // Visitor implementation
    void visit(const Class* node) override;
    void visit(const Field* node) override;
    void visit(const Method* node) override;
    void visit(const Formal* node) override;
    void visit(const BinaryOp* node) override;
    void visit(const UnaryOp* node) override;
    void visit(const Call* node) override;
    void visit(const New* node) override;
    void visit(const Let* node) override;
    void visit(const If* node) override;
    void visit(const While* node) override;
    void visit(const Assign* node) override;
    void visit(const StringLiteral* node) override;
    void visit(const IntegerLiteral* node) override;
    void visit(const BooleanLiteral* node) override;
    void visit(const UnitLiteral* node) override;
    void visit(const Identifier* node) override;
    void visit(const Self* node) override;
    void visit(const Block* node) override;
    
    // Helper methods
    void reportError(const std::string& message);
    
    // LLVM value storage for expression results
    std::map<const Expression*, llvm::Value*> expression_values;
    void setExprValue(const Expression* expr, llvm::Value* value);
    llvm::Value* getExprValue(const Expression* expr);
    
    // Type conversions
    llvm::Type* getLLVMType(const std::string& vsop_type);
    llvm::Value* castIfNeeded(llvm::Value* value, llvm::Type* target_type, const std::string& source_type, const std::string& target_type_name);
    
    // Code generation helpers
    void generateBuiltinFunctions();
    void declareClasses();
    void defineClassStructures();
    void defineMethods();
    void generateClassConstructor(const std::string& class_name, ClassLayout& layout);
    
    // Memory management helpers
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function, const std::string& name, llvm::Type* type);
    
    // Field and method access
    llvm::Value* getField(llvm::Value* object, const std::string& class_name, const std::string& field_name);
    llvm::Value* setField(llvm::Value* object, const std::string& class_name, const std::string& field_name, llvm::Value* value);
    std::pair<llvm::Function*, std::string> getMethod(const std::string& class_name, const std::string& method_name);
    
    // Runtime library functions
    llvm::Function* print_int32_func;
    llvm::Function* print_string_func;
    llvm::Function* print_bool_func;
    llvm::Function* input_int32_func;
    llvm::Function* input_string_func;
    llvm::Function* input_bool_func;
};

} // namespace VSOP

#endif // CODE_GENERATOR_HPP