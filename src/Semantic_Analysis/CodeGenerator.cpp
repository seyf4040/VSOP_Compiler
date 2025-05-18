#include "CodeGenerator.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <system_error>

namespace VSOP {

CodeGenerator::CodeGenerator(const std::string& source_file, bool output_ir)
    : output_ir(output_ir), source_file(source_file) {
    
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>(source_file, *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);
    
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    
    analyzer = SemanticAnalyzer();
}

CodeGenerator::~CodeGenerator() {
    expression_values.clear();
}

bool CodeGenerator::generate(std::shared_ptr<Program> prog) {
    program = prog;
    
    if (!program) {
        reportError("No program to generate code for");
        return false;
    }
    
    if (!analyzer.analyze(program)) {
        const auto& analyzer_errors = analyzer.getErrors();
        errors.insert(errors.end(), analyzer_errors.begin(), analyzer_errors.end());
        return false;
    }
    
    try {
        // Skip Object class if we're using the provided implementation
        using_provided_object = true; // Set this flag
        
        generateBuiltinFunctions();
        
        declareClasses();
        
        defineClassStructures();
        
        defineMethods();
        
        std::string verify_error;
        llvm::raw_string_ostream verify_out(verify_error);
        if (llvm::verifyModule(*module, &verify_out)) {
            reportError("LLVM module verification failed: " + verify_error);
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        reportError(std::string("Exception during code generation: ") + e.what());
        return false;
    } catch (...) {
        reportError("Unknown exception during code generation");
        return false;
    }
}

void CodeGenerator::dumpIR() {
    if (module) {
        // If using provided Object implementation, include its IR
        if (using_provided_object) {
            std::ifstream object_file("object.ll");
            if (!object_file.is_open()) {
                // Try other locations
                std::vector<std::string> search_paths = {
                    "/usr/local/share/vsopc/",
                    "/usr/share/vsopc/",
                    "../",
                    "./",
                    // Add more search paths as needed
                };
                
                for (const auto& path : search_paths) {
                    object_file.open(path + "object.ll");
                    if (object_file.is_open()) break;
                }
            }
            
            if (object_file.is_open()) {
                std::string object_ir((std::istreambuf_iterator<char>(object_file)), 
                                   (std::istreambuf_iterator<char>()));
                object_file.close();
                
                // Output Object IR first
                llvm::outs() << object_ir << "\n";
            } else {
                reportError("Warning: Could not include object.ll in the IR output");
            }
        }
        
        // Output the module IR
        module->print(llvm::outs(), nullptr);
    }
}

bool CodeGenerator::generateExecutable(const std::string& output_file) {
    if (!module) {
        reportError("No module to generate executable from");
        return false;
    }
    
    this->output_file = output_file;
    
    // Write the LLVM IR to a temporary file
    std::string ir_file = output_file + ".ll";
    std::error_code EC;
    llvm::raw_fd_ostream ir_out(ir_file, EC, llvm::sys::fs::OF_None);
    
    if (EC) {
        reportError("Could not open file for writing IR: " + EC.message());
        return false;
    }
    
    module->print(ir_out, nullptr);
    ir_out.close();
    
    // Optimize the module
    llvm::legacy::PassManager pass_manager;
    pass_manager.add(llvm::createPromoteMemoryToRegisterPass());
    pass_manager.add(llvm::createInstructionCombiningPass());
    pass_manager.add(llvm::createReassociatePass());
    pass_manager.add(llvm::createGVNPass());
    pass_manager.add(llvm::createCFGSimplificationPass());
    
    // Target information for code generation
    auto target_triple = llvm::sys::getDefaultTargetTriple();
    module->setTargetTriple(target_triple);
    
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    
    if (!target) {
        reportError("Failed to lookup target: " + error);
        return false;
    }
    
    auto CPU = "generic";
    auto Features = "";
    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>();
    auto target_machine = target->createTargetMachine(target_triple, CPU, Features, opt, RM);
    
    module->setDataLayout(target_machine->createDataLayout());
    
    // Generate object file
    std::string obj_file = output_file + ".o";
    llvm::raw_fd_ostream obj_out(obj_file, EC, llvm::sys::fs::OF_None);
    
    if (EC) {
        reportError("Could not open file for writing object code: " + EC.message());
        return false;
    }
    
    llvm::legacy::PassManager pass;
    auto file_type = llvm::CGFT_ObjectFile;
    
    if (target_machine->addPassesToEmitFile(pass, obj_out, nullptr, file_type)) {
        reportError("Target machine can't emit a file of this type");
        return false;
    }
    
    pass.run(*module);
    obj_out.close();
    
    // Link with object.o to create executable
    // First, check if object.o exists in the current directory or a known location
    std::string object_o = "object.o";
    std::string object_c = "object.c";
    
    // Check for object.o in current directory
    bool object_o_exists = std::ifstream(object_o).good();
    bool object_c_exists = std::ifstream(object_c).good();
    
    std::string compile_command;
    
    if (object_o_exists) {
        // Link with pre-compiled object.o
        compile_command = "clang " + obj_file + " " + object_o + " -o " + output_file;
    } else if (object_c_exists) {
        // Compile object.c and link
        compile_command = "clang " + obj_file + " " + object_c + " -o " + output_file;
    } else {
        // Try to find object.o or object.c in standard locations
        std::vector<std::string> search_paths = {
            "/usr/local/share/vsopc/",
            "/usr/share/vsopc/",
            "../",
            "./",
            // Add more search paths as needed
        };
        
        bool found = false;
        std::string object_path;
        
        for (const auto& path : search_paths) {
            if (std::ifstream(path + object_o).good()) {
                object_path = path + object_o;
                found = true;
                break;
            } else if (std::ifstream(path + object_c).good()) {
                object_path = path + object_c;
                found = true;
                break;
            }
        }
        
        if (found) {
            compile_command = "clang " + obj_file + " " + object_path + " -o " + output_file;
        } else {
            reportError("Could not find object.o or object.c for linking");
            return false;
        }
    }
    
    int result = std::system(compile_command.c_str());
    
    if (result != 0) {
        reportError("Failed to link executable");
        return false;
    }
    
    // Cleanup temporary files
    std::remove(ir_file.c_str());
    std::remove(obj_file.c_str());
    
    return true;
}


const std::vector<std::string>& CodeGenerator::getErrors() const {
    return errors;
}

void CodeGenerator::reportError(const std::string& message) {
    std::string error = "Code generation error: " + message;
    errors.push_back(error);
}

void CodeGenerator::setExprValue(const Expression* expr, llvm::Value* value) {
    expression_values[expr] = value;
}

llvm::Value* CodeGenerator::getExprValue(const Expression* expr) {
    auto it = expression_values.find(expr);
    if (it != expression_values.end()) {
        return it->second;
    }
    reportError("No value for expression");
    return nullptr;
}

llvm::Type* CodeGenerator::getLLVMType(const std::string& vsop_type) {
    if (vsop_type == "int32") {
        return llvm::Type::getInt32Ty(*context);
    } else if (vsop_type == "bool") {
        return llvm::Type::getInt1Ty(*context);
    } else if (vsop_type == "string") {
        return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context));
    } else if (vsop_type == "unit") {
        return llvm::Type::getVoidTy(*context);
    } else {
        auto it = class_layouts.find(vsop_type);
        if (it != class_layouts.end()) {
            return llvm::PointerType::getUnqual(it->second.type);
        }
        reportError("Unknown type: " + vsop_type);
        return nullptr;
    }
}

llvm::Value* CodeGenerator::castIfNeeded(llvm::Value* value, llvm::Type* target_type, 
                                        const std::string& source_type, 
                                        const std::string& target_type_name) {
    if (!value || !target_type) return nullptr;
    
    if (value->getType() == target_type) {
        return value;
    }
    
    if (target_type->isIntegerTy() && value->getType()->isIntegerTy()) {
        if (target_type->isIntegerTy(1)) {
            return builder->CreateICmpNE(value, llvm::ConstantInt::get(value->getType(), 0));
        } else {
            return builder->CreateZExt(value, target_type);
        }
    }
    
    if (target_type->isPointerTy() && value->getType()->isPointerTy()) {
        Type src_type = analyzer.resolveType(source_type);
        Type tgt_type = analyzer.resolveType(target_type_name);
        
        if (!src_type.isError() && !tgt_type.isError() && 
            src_type.conformsTo(tgt_type, analyzer.getClassDefinitions())) {
            return builder->CreateBitCast(value, target_type);
        }
    }
    
    reportError("Cannot cast from " + source_type + " to " + target_type_name);
    return nullptr;
}

llvm::AllocaInst* CodeGenerator::createEntryBlockAlloca(llvm::Function* function, 
                                                       const std::string& name, 
                                                       llvm::Type* type) {
    llvm::IRBuilder<> tmp_builder(&function->getEntryBlock(), 
                               function->getEntryBlock().begin());
    return tmp_builder.CreateAlloca(type, nullptr, name);
}

void CodeGenerator::generateBuiltinFunctions() {
    
    // print_string(s: string): Object
    print_string_func = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*context), 
                              {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context))}, 
                              false),
        llvm::Function::ExternalLinkage,
        "print_string",
        *module
    );
    
    // print_int32(i: int32): Object
    print_int32_func = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*context), {llvm::Type::getInt32Ty(*context)}, false),
        llvm::Function::ExternalLinkage,
        "print_int32",
        *module
    );
    
    // print_bool(b: bool): Object
    print_bool_func = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*context), {llvm::Type::getInt1Ty(*context)}, false),
        llvm::Function::ExternalLinkage,
        "print_bool",
        *module
    );
    
    // input_int32(): int32
    input_int32_func = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), {}, false),
        llvm::Function::ExternalLinkage,
        "input_int32",
        *module
    );
    
    // input_string(): string
    input_string_func = llvm::Function::Create(
        llvm::FunctionType::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context)), {}, false),
        llvm::Function::ExternalLinkage,
        "input_string",
        *module
    );
    
    // input_bool(): bool
    input_bool_func = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt1Ty(*context), {}, false),
        llvm::Function::ExternalLinkage,
        "input_bool",
        *module
    );
}


void CodeGenerator::declareClasses() {
    for (const auto& cls : program->classes) {
        if (!cls) continue;
        
        // Skip Object class if using provided implementation
        if (using_provided_object && cls->name == "Object") continue;
        
        auto class_type = llvm::StructType::create(*context, cls->name + "_struct");
        
        ClassLayout layout;
        layout.type = class_type;
        layout.parent = cls->parent.empty() ? "Object" : cls->parent;
        
        class_layouts[cls->name] = layout;
    }
    
    // If Object isn't defined yet and we're not using the provided implementation, define it
    if (class_layouts.find("Object") == class_layouts.end() && !using_provided_object) {
        auto object_type = llvm::StructType::create(*context, "Object_struct");
        
        ClassLayout layout;
        layout.type = object_type;
        layout.parent = "";
        
        class_layouts["Object"] = layout;
    }
}

void CodeGenerator::defineClassStructures() {
    for (const auto& cls : program->classes) {
        if (!cls) continue;
        
        // Skip Object class if using provided implementation
        if (using_provided_object && cls->name == "Object") continue;
        
        auto& layout = class_layouts[cls->name];
        
        std::vector<llvm::Type*> field_types;
        
        field_types.push_back(llvm::PointerType::getUnqual(llvm::Type::getInt8PtrTy(*context)));
        
        if (!layout.parent.empty() && layout.parent != "Object") {
            auto parent_it = class_layouts.find(layout.parent);
            if (parent_it != class_layouts.end()) {
                const auto& parent_layout = parent_it->second;
                
                auto parent_struct = parent_layout.type;
                auto parent_body = parent_struct->elements();
                for (size_t i = 1; i < parent_body.size(); i++) {
                    field_types.push_back(parent_body[i]);
                }
                
                for (const auto& field_entry : parent_layout.field_indices) {
                    const std::string& field_name = field_entry.first;
                    int field_idx = field_entry.second;
                    layout.field_indices[field_name] = field_idx;
                }
            }
        }
        
        int field_idx = field_types.size();
        for (const auto& field : cls->fields) {
            if (!field) continue;
            
            auto field_type = getLLVMType(field->type);
            field_types.push_back(field_type);
            
            layout.field_indices[field->name] = field_idx++;
        }
        
        layout.type->setBody(field_types);
        
        generateClassConstructor(cls->name, layout);
    }
    
    // If Object class layout exists and is not yet defined (and we're not using provided implementation)
    auto object_it = class_layouts.find("Object");
    if (object_it != class_layouts.end() && !using_provided_object) {
        auto& object_layout = object_it->second;
        if (!object_layout.type->isOpaque()) {
            return;
        }
        
        std::vector<llvm::Type*> field_types;
        field_types.push_back(llvm::PointerType::getUnqual(llvm::Type::getInt8PtrTy(*context)));
        object_layout.type->setBody(field_types);
        
        generateClassConstructor("Object", object_layout);
    }
}

void CodeGenerator::generateClassConstructor(const std::string& class_name, ClassLayout& layout) {
    // Creates constructor function: Type* new_ClassName()
    auto ctor_type = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(layout.type),
        {},
        false
    );
    
    auto ctor_func = llvm::Function::Create(
        ctor_type,
        llvm::Function::ExternalLinkage,
        "new_" + class_name,
        *module
    );
    
    layout.constructor = ctor_func;
    
    auto entry_block = llvm::BasicBlock::Create(*context, "entry", ctor_func);
    builder->SetInsertPoint(entry_block);
    
    // Allocates memory for object
    auto size = llvm::ConstantExpr::getSizeOf(layout.type);
    auto malloc_func = module->getOrInsertFunction("malloc",
        llvm::FunctionType::get(
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context)),
            {llvm::Type::getInt64Ty(*context)},
            false
        )
    );
    
    auto mem_ptr = builder->CreateCall(malloc_func, {builder->CreateZExt(size, llvm::Type::getInt64Ty(*context))});
    auto obj_ptr = builder->CreateBitCast(mem_ptr, llvm::PointerType::getUnqual(layout.type));
    
    auto null_vtable = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8PtrTy(*context)));
    auto vtable_ptr = builder->CreateStructGEP(layout.type, obj_ptr, 0);
    builder->CreateStore(null_vtable, vtable_ptr);
    
    for (const auto& field_entry : layout.field_indices) {
        const std::string& field_name = field_entry.first;
        int field_idx = field_entry.second;
    
        auto field_ptr = builder->CreateStructGEP(layout.type, obj_ptr, field_idx);
        
        auto field_type = layout.type->getElementType(field_idx);
        
        if (field_type->isIntegerTy()) {
            builder->CreateStore(llvm::ConstantInt::get(field_type, 0), field_ptr);
        } 
        else if (field_type->isPointerTy()) {
            builder->CreateStore(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(field_type)), field_ptr);
        }
    }
    
    builder->CreateRet(obj_ptr);
}

void CodeGenerator::defineMethods() {
    for (const auto& cls : program->classes) {
        if (!cls) continue;
        
        current_class = cls->name;
        auto& layout = class_layouts[cls->name];
        
        for (const auto& method : cls->methods) {
            if (!method) continue;
            
            std::vector<llvm::Type*> param_types;
            
            param_types.push_back(llvm::PointerType::getUnqual(layout.type));
            
            for (const auto& formal : method->formals) {
                if (!formal) continue;
                
                param_types.push_back(getLLVMType(formal->type));
            }
            
            llvm::Type* return_type = getLLVMType(method->return_type);
            if (method->return_type == "unit") {
                return_type = llvm::Type::getVoidTy(*context);
            }
            
            auto func_type = llvm::FunctionType::get(return_type, param_types, false);
            
            auto func = llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                cls->name + "_" + method->name,
                *module
            );
            
            auto arg_it = func->arg_begin();
            arg_it->setName("self");
            ++arg_it;
            
            for (const auto& formal : method->formals) {
                if (!formal) continue;
                
                arg_it->setName(formal->name);
                ++arg_it;
            }
            
            layout.methods[method->name] = func;
        }
    }
    
    for (const auto& cls : program->classes) {
        if (!cls) continue;
        
        current_class = cls->name;
        auto& layout = class_layouts[cls->name];
        
        for (const auto& method : cls->methods) {
            if (!method) continue;
            
            current_method = method->name;
            
            auto func = layout.methods[method->name];
            
            auto entry_block = llvm::BasicBlock::Create(*context, "entry", func);
            builder->SetInsertPoint(entry_block);
            
            named_values.clear();
            
            auto self_val = func->arg_begin();
            current_self = createEntryBlockAlloca(func, "self_ptr", self_val->getType());
            builder->CreateStore(self_val, current_self);
            
            auto arg_it = func->arg_begin();
            ++arg_it;
            
            for (const auto& formal : method->formals) {
                if (!formal) continue;
                
                auto alloca = createEntryBlockAlloca(func, formal->name, getLLVMType(formal->type));
                builder->CreateStore(arg_it, alloca);
                named_values[formal->name] = alloca;
                
                ++arg_it;
            }
            
            if (method->body) {
                method->body->accept(this);
                
                auto ret_val = getExprValue(method->body.get());
                
                if (method->return_type == "unit") {
                    builder->CreateRetVoid();
                } else if (!builder->GetInsertBlock()->getTerminator()) {
                    builder->CreateRet(ret_val);
                }
            } else {
                if (method->return_type == "unit") {
                    builder->CreateRetVoid();
                } else {
                    auto return_type = getLLVMType(method->return_type);
                    if (return_type->isIntegerTy()) {
                        builder->CreateRet(llvm::ConstantInt::get(return_type, 0));
                    } else if (return_type->isPointerTy()) {
                        builder->CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(return_type)));
                    } else {
                        reportError("Cannot generate default return value for method " + method->name);
                    }
                }
            }
        }
    }
    
    auto main_func_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), {}, false);
    auto main_func = llvm::Function::Create(main_func_type, llvm::Function::ExternalLinkage, "main", *module);
    
    auto main_entry = llvm::BasicBlock::Create(*context, "entry", main_func);
    builder->SetInsertPoint(main_entry);
    
    auto main_it = class_layouts.find("Main");
    if (main_it != class_layouts.end()) {
        auto& main_layout = main_it->second;
        auto main_method_it = main_layout.methods.find("main");
        
        if (main_method_it != main_layout.methods.end()) {
            auto main_obj = builder->CreateCall(main_layout.constructor);
            
            auto call_result = builder->CreateCall(main_method_it->second, {main_obj});
            
            if (main_method_it->second->getReturnType()->isVoidTy()) {
                builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0));
            } else {
                builder->CreateRet(call_result);
            }
        } else {
            reportError("Main class does not have a main method");
            builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 1));
        }
    } else {
        reportError("Main class not found");
        builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 1));
    }
}

// Visitors

void CodeGenerator::visit(const Class* node) {
    // Classes are handled in declareClasses, defineClassStructures, and defineMethods
    (void)node;
}

void CodeGenerator::visit(const Field* node) {
    // Fields are handled in defineClassStructures
    (void)node;
}

void CodeGenerator::visit(const Method* node) {
    // Methods are handled in defineMethods
    (void)node;
}

void CodeGenerator::visit(const Formal* node) {
    // Formals are handled in defineMethods
    (void)node;
}

void CodeGenerator::visit(const StringLiteral* node) {
    auto str_const = builder->CreateGlobalStringPtr(node->value);
    setExprValue(node, str_const);
}

void CodeGenerator::visit(const IntegerLiteral* node) {
    auto val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), node->value);
    setExprValue(node, val);
}

void CodeGenerator::visit(const BooleanLiteral* node) {
    auto val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), node->value ? 1 : 0);
    setExprValue(node, val);
}

void CodeGenerator::visit(const UnitLiteral* node) {
    // Unit is represented as void, so no value to set
    // For expressions, we'll use a dummy value
    setExprValue(node, llvm::UndefValue::get(llvm::Type::getInt32Ty(*context)));
}

void CodeGenerator::visit(const BinaryOp* node) {
    node->left->accept(this);
    node->right->accept(this);
    
    auto left_val = getExprValue(node->left.get());
    auto right_val = getExprValue(node->right.get());
    
    if (!left_val || !right_val) return;
    
    llvm::Value* result = nullptr;
    
    if (node->op == "+") {
        result = builder->CreateAdd(left_val, right_val, "addtmp");
    } else if (node->op == "-") {
        result = builder->CreateSub(left_val, right_val, "subtmp");
    } else if (node->op == "*") {
        result = builder->CreateMul(left_val, right_val, "multmp");
    } else if (node->op == "/") {
        auto zero = llvm::ConstantInt::get(right_val->getType(), 0);
        auto is_zero = builder->CreateICmpEQ(right_val, zero, "iszero");
        
        auto div_bb = llvm::BasicBlock::Create(*context, "div", builder->GetInsertBlock()->getParent());
        auto cont_bb = llvm::BasicBlock::Create(*context, "cont", builder->GetInsertBlock()->getParent());
        
        builder->CreateCondBr(is_zero, cont_bb, div_bb);
        
        // Division block
        builder->SetInsertPoint(div_bb);
        auto div_result = builder->CreateSDiv(left_val, right_val, "divtmp");
        builder->CreateBr(cont_bb);
        
        // Continue block
        builder->SetInsertPoint(cont_bb);
        auto phi = builder->CreatePHI(left_val->getType(), 2, "divresult");
        phi->addIncoming(llvm::ConstantInt::get(left_val->getType(), 0), builder->GetInsertBlock()->getParent()->getEntryBlock().getTerminator()->getSuccessor(0));
        phi->addIncoming(div_result, div_bb);
        
        result = phi;
    } else if (node->op == "^") {
        // Exponentiation using repeated multiplication
        // For simplicity, we'll only implement positive exponents
        
        auto zero = llvm::ConstantInt::get(right_val->getType(), 0);
        auto one = llvm::ConstantInt::get(left_val->getType(), 1);
        
        // Check for exponent <= 0
        auto is_neg = builder->CreateICmpSLE(right_val, zero, "isneg");
        
        auto pow_bb = llvm::BasicBlock::Create(*context, "powstart", builder->GetInsertBlock()->getParent());
        auto loop_bb = llvm::BasicBlock::Create(*context, "powloop", builder->GetInsertBlock()->getParent());
        auto done_bb = llvm::BasicBlock::Create(*context, "powdone", builder->GetInsertBlock()->getParent());
        
        builder->CreateCondBr(is_neg, done_bb, pow_bb);
        
        // Power calculation setup
        builder->SetInsertPoint(pow_bb);
        auto result_ptr = createEntryBlockAlloca(builder->GetInsertBlock()->getParent(), "powresult", left_val->getType());
        auto counter_ptr = createEntryBlockAlloca(builder->GetInsertBlock()->getParent(), "powcounter", right_val->getType());
        
        builder->CreateStore(one, result_ptr);
        builder->CreateStore(right_val, counter_ptr);
        builder->CreateBr(loop_bb);
        
        // Loop block
        builder->SetInsertPoint(loop_bb);
        auto current_result = builder->CreateLoad(result_ptr->getAllocatedType(), result_ptr);
        auto current_counter = builder->CreateLoad(counter_ptr->getAllocatedType(), counter_ptr);
        
        auto is_done = builder->CreateICmpEQ(current_counter, zero, "isdone");
        builder->CreateCondBr(is_done, done_bb, loop_bb);
        
        // Decrements counter and multiplies result
        auto new_counter = builder->CreateSub(current_counter, llvm::ConstantInt::get(current_counter->getType(), 1));
        auto new_result = builder->CreateMul(current_result, left_val);
        
        builder->CreateStore(new_counter, counter_ptr);
        builder->CreateStore(new_result, result_ptr);
        
        builder->SetInsertPoint(done_bb);
        
        // Phi node to select between 1 (if exponent <= 0) and the calculated result
        auto phi = builder->CreatePHI(left_val->getType(), 2, "powresult");
        phi->addIncoming(one, builder->GetInsertBlock()->getParent()->getEntryBlock().getTerminator()->getSuccessor(0));
        phi->addIncoming(current_result, loop_bb);
        
        result = phi;
    } else if (node->op == "<") {
        result = builder->CreateICmpSLT(left_val, right_val, "lttmp");
    } else if (node->op == "<=") {
        result = builder->CreateICmpSLE(left_val, right_val, "letmp");
    } else if (node->op == "=") {
        result = builder->CreateICmpEQ(left_val, right_val, "eqtmp");
    } else if (node->op == "and") {
        // Short-circuit And
        auto and_bb = llvm::BasicBlock::Create(*context, "and", builder->GetInsertBlock()->getParent());
        auto cont_bb = llvm::BasicBlock::Create(*context, "cont", builder->GetInsertBlock()->getParent());
        
        builder->CreateCondBr(left_val, and_bb, cont_bb);
        
        // Second operand evaluation
        builder->SetInsertPoint(and_bb);
        builder->CreateBr(cont_bb);
        
        // Result
        builder->SetInsertPoint(cont_bb);
        auto phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2, "andresult");
        phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), 0), builder->GetInsertBlock()->getParent()->getEntryBlock().getTerminator()->getSuccessor(0));
        phi->addIncoming(right_val, and_bb);
        
        result = phi;
    } else {
        reportError("Unsupported binary operator: " + node->op);
        return;
    }
    
    setExprValue(node, result);
}

void CodeGenerator::visit(const UnaryOp* node) {
    node->expr->accept(this);
    auto expr_val = getExprValue(node->expr.get());
    
    if (!expr_val) return;
    
    llvm::Value* result = nullptr;
    
    if (node->op == "-") {
        result = builder->CreateNeg(expr_val, "negtmp");
    } else if (node->op == "not") {
        result = builder->CreateNot(expr_val, "nottmp");
    } else if (node->op == "isnull") {
        auto null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(expr_val->getType()));
        result = builder->CreateICmpEQ(expr_val, null_ptr, "isnulltmp");
    } else {
        reportError("Unsupported unary operator: " + node->op);
        return;
    }
    
    setExprValue(node, result);
}

void CodeGenerator::visit(const Call* node) {
    llvm::Value* obj_val = nullptr;
    std::string class_name = "";
    
    if (node->object) {
        node->object->accept(this);
        obj_val = getExprValue(node->object.get());
        
        if (!obj_val) return;
        
        if (const Identifier* id = dynamic_cast<const Identifier*>(node->object.get())) {
            auto it = named_values.find(id->name);
            if (it != named_values.end()) {
                // Variable is in scope
                class_name = id->name;
            } else {
                // Check if it's a field
                auto field_result = analyzer.findFieldType(current_class, id->name);
                bool found = field_result.first;
                Type field_type = field_result.second;
                if (found) {
                    class_name = field_type.toString();
                }
            }
        } else if (const Self* self = dynamic_cast<const Self*>(node->object.get())) {
            // Self object
            class_name = current_class;
        } else if (const New* new_expr = dynamic_cast<const New*>(node->object.get())) {
            // New expression
            class_name = new_expr->type_name;
        }
        
        // If we couldn't determine the class, tries to infer from the LLVM type
        if (class_name.empty()) {
            auto llvm_type = obj_val->getType();
            if (llvm_type->isPointerTy()) {
                auto pointed_type = llvm_type->getPointerElementType();
                if (pointed_type->isStructTy()) {
                    auto struct_name = pointed_type->getStructName().str();
                    if (struct_name.length() > 7 && struct_name.substr(struct_name.length() - 7) == "_struct") {
                        class_name = struct_name.substr(0, struct_name.length() - 7);
                    }
                }
            }
        }
    } else {
        obj_val = builder->CreateLoad(current_self->getAllocatedType(), current_self);
        class_name = current_class;
    }
    
    if (!obj_val) {
        reportError("Could not get object for method call");
        return;
    }
    
    auto method_result = getMethod(class_name, node->method_name);
    llvm::Function* method_func = method_result.first;
    std::string method_class = method_result.second;
    
    if (!method_func) {
        reportError("Method not found: " + class_name + "." + node->method_name);
        return;
    }
    
    std::vector<llvm::Value*> arg_vals;
    arg_vals.push_back(obj_val);
    
    for (const auto& arg : node->arguments) {
        if (!arg) {
            reportError("Null argument in method call");
            return;
        }
        
        arg->accept(this);
        auto arg_val = getExprValue(arg.get());
        
        if (!arg_val) {
            reportError("Could not evaluate argument in method call");
            return;
        }
        
        arg_vals.push_back(arg_val);
    }
    
    auto call_result = builder->CreateCall(method_func, arg_vals);
    
    if (method_class == "Object") {
        if (node->method_name == "print" || node->method_name == "printInt32" || node->method_name == "printBool") {
            // These methods return Object, but the underlying LLVM functions return void
            // Use a dummy value
            setExprValue(node, llvm::UndefValue::get(llvm::Type::getInt32Ty(*context)));
            return;
        }
    }
    
    setExprValue(node, call_result);
}

void CodeGenerator::visit(const New* node) {
    auto it = class_layouts.find(node->type_name);
    if (it == class_layouts.end()) {
        reportError("Unknown class: " + node->type_name);
        return;
    }
    
    auto constructor = it->second.constructor;
    auto result = builder->CreateCall(constructor);
    
    setExprValue(node, result);
}

void CodeGenerator::visit(const Let* node) {
    auto var_type = getLLVMType(node->type);
    auto alloca = createEntryBlockAlloca(builder->GetInsertBlock()->getParent(), node->name, var_type);
    
    if (node->init_expr) {
        node->init_expr->accept(this);
        auto init_val = getExprValue(node->init_expr.get());
        
        if (!init_val) {
            reportError("Could not evaluate let initialization expression");
            return;
        }
        
        builder->CreateStore(init_val, alloca);
    } else {
        if (var_type->isIntegerTy()) {
            builder->CreateStore(llvm::ConstantInt::get(var_type, 0), alloca);
        } else if (var_type->isPointerTy()) {
            builder->CreateStore(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(var_type)), alloca);
        }
    }
    
    auto old_value = named_values[node->name];
    named_values[node->name] = alloca;
    
    if (node->scope_expr) {
        node->scope_expr->accept(this);
        auto scope_val = getExprValue(node->scope_expr.get());
        
        if (!scope_val) {
            reportError("Could not evaluate let scope expression");
            return;
        }
        
        setExprValue(node, scope_val);
    } else {
        setExprValue(node, llvm::UndefValue::get(llvm::Type::getInt32Ty(*context)));
    }
    
    if (old_value) {
        named_values[node->name] = old_value;
    } else {
        named_values.erase(node->name);
    }
}

void CodeGenerator::visit(const If* node) {
    node->condition->accept(this);
    auto cond_val = getExprValue(node->condition.get());
    
    if (!cond_val) {
        reportError("Could not evaluate if condition");
        return;
    }
    
    // Creates basic blocks
    auto func = builder->GetInsertBlock()->getParent();
    auto then_bb = llvm::BasicBlock::Create(*context, "then", func);
    auto else_bb = llvm::BasicBlock::Create(*context, "else");
    auto merge_bb = llvm::BasicBlock::Create(*context, "ifcont");
    
    builder->CreateCondBr(cond_val, then_bb, else_bb);
    
    // Then block
    builder->SetInsertPoint(then_bb);
    node->then_expr->accept(this);
    auto then_val = getExprValue(node->then_expr.get());
    
    if (!then_val) {
        reportError("Could not evaluate then expression");
        return;
    }
    
    builder->CreateBr(merge_bb);
    then_bb = builder->GetInsertBlock();
    
    // Else block
    func->getBasicBlockList().push_back(else_bb);
    builder->SetInsertPoint(else_bb);
    
    llvm::Value* else_val = nullptr;
    if (node->else_expr) {
        node->else_expr->accept(this);
        else_val = getExprValue(node->else_expr.get());
        
        if (!else_val) {
            reportError("Could not evaluate else expression");
            return;
        }
    } else {
        // No else branch - uses unit value
        else_val = llvm::UndefValue::get(llvm::Type::getInt32Ty(*context));
    }
    
    builder->CreateBr(merge_bb);
    else_bb = builder->GetInsertBlock(); // It could have changed
    
    // Merge block
    func->getBasicBlockList().push_back(merge_bb);
    builder->SetInsertPoint(merge_bb);
    
    // Create phi node if we have a non-void result
    if (!then_val->getType()->isVoidTy()) {
        auto phi = builder->CreatePHI(then_val->getType(), 2, "iftmp");
        phi->addIncoming(then_val, then_bb);
        phi->addIncoming(else_val, else_bb);
        
        setExprValue(node, phi);
    } else {
        // Void result - uses unit value
        setExprValue(node, llvm::UndefValue::get(llvm::Type::getInt32Ty(*context)));
    }
}

void CodeGenerator::visit(const While* node) {
    // Create basic blocks
    auto func = builder->GetInsertBlock()->getParent();
    auto cond_bb = llvm::BasicBlock::Create(*context, "whilecond", func);
    auto body_bb = llvm::BasicBlock::Create(*context, "whilebody");
    auto end_bb = llvm::BasicBlock::Create(*context, "whileend");
    
    // Saves previous loop exit block for nested loops
    loop_exit_blocks.push(end_bb);
    
    // Jumps to condition block
    builder->CreateBr(cond_bb);
    
    // Condition block
    builder->SetInsertPoint(cond_bb);
    node->condition->accept(this);
    auto cond_val = getExprValue(node->condition.get());
    
    if (!cond_val) {
        reportError("Could not evaluate while condition");
        return;
    }
    
    builder->CreateCondBr(cond_val, body_bb, end_bb);
    
    // Body block
    func->getBasicBlockList().push_back(body_bb);
    builder->SetInsertPoint(body_bb);
    
    if (node->body) {
        node->body->accept(this);
    }
    
    // Jumps back to condition
    builder->CreateBr(cond_bb);
    
    // End block
    func->getBasicBlockList().push_back(end_bb);
    builder->SetInsertPoint(end_bb);
    
    // Pop the loop exit block
    loop_exit_blocks.pop();
    
    // While always returns unit
    setExprValue(node, llvm::UndefValue::get(llvm::Type::getInt32Ty(*context)));
}

void CodeGenerator::visit(const Assign* node) {
    // Gets the value to assign
    node->expr->accept(this);
    auto val = getExprValue(node->expr.get());
    
    if (!val) {
        reportError("Could not evaluate assignment expression");
        return;
    }
    
    // Checks if the variable is a local variable
    auto it = named_values.find(node->name);
    if (it != named_values.end()) {
        // Local variable
        builder->CreateStore(val, it->second);
    } else {
        // Checks if it's a field
        auto self_val = builder->CreateLoad(current_self->getAllocatedType(), current_self);
        
        if (!setField(self_val, current_class, node->name, val)) {
            reportError("Could not assign to " + node->name);
            return;
        }
    }
    
    setExprValue(node, val);
}

void CodeGenerator::visit(const Identifier* node) {
    // Checks if the identifier is a local variable
    auto it = named_values.find(node->name);
    if (it != named_values.end()) {
        // Local variable
        auto val = builder->CreateLoad(it->second->getAllocatedType(), it->second, node->name.c_str());
        setExprValue(node, val);
        return;
    }
    
    // Check if it's a field
    auto self_val = builder->CreateLoad(current_self->getAllocatedType(), current_self);
    auto field_val = getField(self_val, current_class, node->name);
    
    if (!field_val) {
        reportError("Could not find identifier: " + node->name);
        return;
    }
    
    setExprValue(node, field_val);
}

void CodeGenerator::visit(const Self* node) {
    auto self_val = builder->CreateLoad(current_self->getAllocatedType(), current_self);
    setExprValue(node, self_val);
}

void CodeGenerator::visit(const Block* node) {
    // Evaluates each expression in the block
    llvm::Value* result = nullptr;
    
    for (const auto& expr : node->expressions) {
        if (!expr) {
            reportError("Null expression in block");
            continue;
        }
        
        expr->accept(this);
        result = getExprValue(expr.get());
        
        if (!result) {
            reportError("Could not evaluate expression in block");
            return;
        }
    }
    
    // Block returns the value of the last expression, or unit if empty
    if (result) {
        setExprValue(node, result);
    } else {
        setExprValue(node, llvm::UndefValue::get(llvm::Type::getInt32Ty(*context)));
    }
}

llvm::Value* CodeGenerator::getField(llvm::Value* object, const std::string& class_name, const std::string& field_name) {
    auto it = class_layouts.find(class_name);
    if (it == class_layouts.end()) {
        reportError("Unknown class: " + class_name);
        return nullptr;
    }
    
    const auto& layout = it->second;
    
    auto field_it = layout.field_indices.find(field_name);
    if (field_it != layout.field_indices.end()) {
        auto field_ptr = builder->CreateStructGEP(layout.type, object, field_it->second, "field." + field_name);
        return builder->CreateLoad(field_ptr->getType()->getPointerElementType(), field_ptr, field_name.c_str());
    }
    
    // Checks parent class if it exists
    if (!layout.parent.empty() && layout.parent != "Object") {
        // Casts object to parent type and try again
        auto parent_it = class_layouts.find(layout.parent);
        if (parent_it != class_layouts.end()) {
            auto parent_type = parent_it->second.type;
            auto parent_obj = builder->CreateBitCast(object, llvm::PointerType::getUnqual(parent_type));
            return getField(parent_obj, layout.parent, field_name);
        }
    }
    
    reportError("Field not found: " + field_name + " in class " + class_name);
    return nullptr;
}

llvm::Value* CodeGenerator::setField(llvm::Value* object, const std::string& class_name, const std::string& field_name, llvm::Value* value) {
    auto it = class_layouts.find(class_name);
    if (it == class_layouts.end()) {
        reportError("Unknown class: " + class_name);
        return nullptr;
    }
    
    const auto& layout = it->second;
    
    // Checks if the field exists in this class
    auto field_it = layout.field_indices.find(field_name);
    if (field_it != layout.field_indices.end()) {
        auto field_ptr = builder->CreateStructGEP(layout.type, object, field_it->second, "field." + field_name);
        builder->CreateStore(value, field_ptr);
        return value;
    }
    
    if (!layout.parent.empty() && layout.parent != "Object") {
        auto parent_it = class_layouts.find(layout.parent);
        if (parent_it != class_layouts.end()) {
            auto parent_type = parent_it->second.type;
            auto parent_obj = builder->CreateBitCast(object, llvm::PointerType::getUnqual(parent_type));
            return setField(parent_obj, layout.parent, field_name, value);
        }
    }
    
    reportError("Field not found: " + field_name + " in class " + class_name);
    return nullptr;
}

std::pair<llvm::Function*, std::string> CodeGenerator::getMethod(const std::string& class_name, const std::string& method_name) {
    // Handle Object methods specially when using provided implementation
    if (using_provided_object && class_name == "Object") {
        if (method_name == "print") {
            return {print_string_func, "Object"};
        } else if (method_name == "printBool") {
            return {print_bool_func, "Object"};
        } else if (method_name == "printInt32") {
            return {print_int32_func, "Object"};
        } else if (method_name == "inputInt32") {
            return {input_int32_func, "Object"};
        } else if (method_name == "inputLine" || method_name == "inputString") {
            return {input_string_func, "Object"};
        } else if (method_name == "inputBool") {
            return {input_bool_func, "Object"};
        }
    }
    
    // For other classes or methods, look in class hierarchy
    std::string current_class = class_name;
    while (!current_class.empty()) {
        auto class_it = class_layouts.find(current_class);
        if (class_it == class_layouts.end()) {
            break;
        }
        
        const auto& layout = class_it->second;
        auto method_it = layout.methods.find(method_name);
        
        if (method_it != layout.methods.end()) {
            return {method_it->second, current_class};
        }
        
        current_class = layout.parent;
        if (current_class == "Object" && using_provided_object) {
            // If we're about to check Object and we're using the provided implementation,
            // check for Object methods
            if (method_name == "print") {
                return {print_string_func, "Object"};
            } else if (method_name == "printBool") {
                return {print_bool_func, "Object"};
            } else if (method_name == "printInt32") {
                return {print_int32_func, "Object"};
            } else if (method_name == "inputInt32") {
                return {input_int32_func, "Object"};
            } else if (method_name == "inputLine" || method_name == "inputString") {
                return {input_string_func, "Object"};
            } else if (method_name == "inputBool") {
                return {input_bool_func, "Object"};
            }
            break;
        }
        if (current_class == "Object") {
            break;
        }
    }
    
    // Method not found
    return {nullptr, ""};
}

}