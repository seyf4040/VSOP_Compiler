#ifndef CODE_GENERATOR_HPP
#define CODE_GENERATOR_HPP

#include "AST.hpp"
#include "SemanticAnalyzer.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Verifier.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace VSOP {

class CodeGenerator {
public:
    CodeGenerator(const std::string& source_file, const std::string& module_name = "vsop_module");
    ~CodeGenerator();
    
    // Generate LLVM IR from the AST
    bool generate(std::shared_ptr<Program> program, bool include_runtime = true);
    
    // Output the generated LLVM IR
    void dumpIR(std::ostream& os);
    
    // Write the native executable
    bool writeNativeExecutable(const std::string& output_file);

    // Get error messages
    const std::vector<std::string>& getErrors() const { return errors; }
    
private:
    std::shared_ptr<Program> program;
    
    // Source file information
    std::string source_file;
    std::string module_name;
    
    // LLVM context and module
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // VTables
    std::unordered_map<std::string, llvm::StructType*> vtable_types;
    std::unordered_map<std::string, llvm::GlobalVariable*> vtable_globals;
        
    // Error handling
    std::vector<std::string> errors;
    
    // Semantic analyzer for type information
    SemanticAnalyzer analyzer;
    
    // Class and method information
    std::unordered_map<std::string, llvm::StructType*> class_types;     // Class name -> LLVM struct type
    std::unordered_map<std::string, llvm::Type*> primitive_types;       // Primitive type name -> LLVM type
    std::unordered_map<std::string, llvm::Function*> methods;           // Method name -> LLVM function
    std::unordered_map<std::string, std::vector<std::string>> vtables;  // Class name -> method list
    
    // Current context for code generation
    std::string current_class;
    llvm::Function* current_function;
    std::unordered_map<std::string, llvm::Value*> current_vars;  // Variable name -> LLVM value

    // Helper methods
    void reportError(const std::string& message);
    void initPrimitiveTypes();
    void includeRuntimeCode();
    void declareRuntimeMethod(const std::string& name, llvm::Type* returnType, const std::vector<llvm::Type*>& paramTypes);
    llvm::Type* getLLVMType(const std::string& vsop_type);
    llvm::Value* createStringConstant(const std::string& str);

    // Code generation passes
    void generateClassTypes();
    void generateClassVTables();
    void generateClassMethods();
    void generateMethodBodies();
    void generateMainEntryPoint();

    // Expression code generation
    llvm::Value* generateExpression(const Expression* expr);
    llvm::Value* generateBinaryOp(const BinaryOp* binop);
    llvm::Value* generateUnaryOp(const UnaryOp* unop);
    llvm::Value* generateCall(const Call* call);
    llvm::Value* generateNew(const New* newExpr);
    llvm::Value* generateLet(const Let* letExpr);
    llvm::Value* generateIf(const If* ifExpr);
    llvm::Value* generateWhile(const While* whileExpr);
    llvm::Value* generateAssign(const Assign* assignExpr);
    llvm::Value* generateBlock(const Block* blockExpr);
    llvm::Value* generateLiteral(const Literal* literal);
    llvm::Value* generateIdentifier(const Identifier* id);
    llvm::Value* generateSelf(const Self* self);
};

} // namespace VSOP

#endif // CODE_GENERATOR_HPP