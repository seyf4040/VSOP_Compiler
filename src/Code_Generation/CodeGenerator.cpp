#include "CodeGenerator.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace VSOP {

// Constructor
CodeGenerator::CodeGenerator(const std::string& source_file, const std::string& module_name)
    : source_file(source_file), module_name(module_name) {
    
    // Initialize LLVM context, module and builder
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>(module_name, *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);
    
    // Initialize primitive types
    initPrimitiveTypes();
}

CodeGenerator::~CodeGenerator() {
    // Cleanup is handled by unique_ptr's destructors
}

void CodeGenerator::initPrimitiveTypes() {
    // Map VSOP primitive types to LLVM types
    primitive_types["int32"] = llvm::Type::getInt32Ty(*context);
    primitive_types["bool"] = llvm::Type::getInt1Ty(*context);
    primitive_types["string"] = llvm::Type::getInt8PtrTy(*context);  // C-style string
    primitive_types["unit"] = llvm::Type::getVoidTy(*context);
}

// Generate LLVM IR from the AST
bool CodeGenerator::generate(std::shared_ptr<Program> prog, bool include_runtime) {
    this->program = prog;
    if (!program) {
        reportError("No program to generate code for");
        return false;
    }
    
    // Perform semantic analysis to ensure the program is well-typed
    if (!analyzer.analyze(program)) {
        for (const auto& error : analyzer.getErrors()) {
            errors.push_back(error);
        }
        return false;
    }
    
    try {
        // Include runtime code if requested
        if (include_runtime) {
            includeRuntimeCode();
        }
        
        // Generate code in multiple passes
        generateClassTypes();
        generateClassVTables();
        generateClassMethods();
        generateMethodBodies();
        generateMainEntryPoint();
        
        // Verify the generated code
        std::string verify_error;
        llvm::raw_string_ostream verify_error_stream(verify_error);
        if (llvm::verifyModule(*module, &verify_error_stream)) {
            reportError("LLVM module verification failed: " + verify_error);
            return false;
        }
        
        return errors.empty();
    }
    catch (const std::exception& e) {
        reportError(std::string("Exception during code generation: ") + e.what());
        return false;
    }
    catch (...) {
        reportError("Unknown exception during code generation");
        return false;
    }
}

void CodeGenerator::includeRuntimeCode() {
    // Following the runtime README.md, we'll take the approach of declaring the Object class
    // interfaces without implementing them, and then linking with the runtime implementation.
    
    // Object struct type (forward declaration)
    llvm::StructType* objectType = llvm::StructType::create(*context, "Object");
    class_types["Object"] = objectType;
    
    // Object vtable struct type (forward declaration)
    llvm::StructType* objectVTableType = llvm::StructType::create(*context, "ObjectVTable");
    
    // Define Object struct: { ObjectVTable* }
    objectType->setBody(llvm::PointerType::get(objectVTableType, 0));
    
    // Define Object vtable struct: function pointers for all methods
    std::vector<llvm::Type*> vtable_methods;
    
    // print(Object* self, const char* s) -> Object*
    vtable_methods.push_back(
        llvm::PointerType::get(
            llvm::FunctionType::get(
                llvm::PointerType::get(objectType, 0),
                {llvm::PointerType::get(objectType, 0), llvm::Type::getInt8PtrTy(*context)},
                false),
            0));
    
    // printBool(Object* self, bool b) -> Object*
    vtable_methods.push_back(
        llvm::PointerType::get(
            llvm::FunctionType::get(
                llvm::PointerType::get(objectType, 0),
                {llvm::PointerType::get(objectType, 0), llvm::Type::getInt1Ty(*context)},
                false),
            0));
    
    // printInt32(Object* self, int32_t i) -> Object*
    vtable_methods.push_back(
        llvm::PointerType::get(
            llvm::FunctionType::get(
                llvm::PointerType::get(objectType, 0),
                {llvm::PointerType::get(objectType, 0), llvm::Type::getInt32Ty(*context)},
                false),
            0));
    
    // inputLine(Object* self) -> char*
    vtable_methods.push_back(
        llvm::PointerType::get(
            llvm::FunctionType::get(
                llvm::Type::getInt8PtrTy(*context),
                {llvm::PointerType::get(objectType, 0)},
                false),
            0));
    
    // inputBool(Object* self) -> bool
    vtable_methods.push_back(
        llvm::PointerType::get(
            llvm::FunctionType::get(
                llvm::Type::getInt1Ty(*context),
                {llvm::PointerType::get(objectType, 0)},
                false),
            0));
    
    // inputInt32(Object* self) -> int32_t
    vtable_methods.push_back(
        llvm::PointerType::get(
            llvm::FunctionType::get(
                llvm::Type::getInt32Ty(*context),
                {llvm::PointerType::get(objectType, 0)},
                false),
            0));
    
    // Set the body of the vtable type
    objectVTableType->setBody(vtable_methods);
    
    // Declare the methods from the runtime (matching the declarations in object.h)
    declareRuntimeMethod("Object__print", 
        llvm::PointerType::get(objectType, 0), 
        {llvm::PointerType::get(objectType, 0), llvm::Type::getInt8PtrTy(*context)});
    
    declareRuntimeMethod("Object__printBool", 
        llvm::PointerType::get(objectType, 0), 
        {llvm::PointerType::get(objectType, 0), llvm::Type::getInt1Ty(*context)});
    
    declareRuntimeMethod("Object__printInt32", 
        llvm::PointerType::get(objectType, 0), 
        {llvm::PointerType::get(objectType, 0), llvm::Type::getInt32Ty(*context)});
    
    declareRuntimeMethod("Object__inputLine", 
        llvm::Type::getInt8PtrTy(*context), 
        {llvm::PointerType::get(objectType, 0)});
    
    declareRuntimeMethod("Object__inputBool", 
        llvm::Type::getInt1Ty(*context), 
        {llvm::PointerType::get(objectType, 0)});
    
    declareRuntimeMethod("Object__inputInt32", 
        llvm::Type::getInt32Ty(*context), 
        {llvm::PointerType::get(objectType, 0)});
    
    // Constructor and initializer
    declareRuntimeMethod("Object___new", 
        llvm::PointerType::get(objectType, 0), 
        {});
    
    declareRuntimeMethod("Object___init", 
        llvm::PointerType::get(objectType, 0), 
        {llvm::PointerType::get(objectType, 0)});
    
    // The global vtable instance is also defined in the runtime
    new llvm::GlobalVariable(
        *module, 
        objectVTableType, 
        true, 
        llvm::GlobalValue::ExternalLinkage, 
        nullptr, 
        "Object___vtable");
}

// Helper method to declare runtime methods
void CodeGenerator::declareRuntimeMethod(const std::string& name, llvm::Type* returnType, 
                                         const std::vector<llvm::Type*>& paramTypes) {
    llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, paramTypes, false);
    llvm::Function* func = llvm::Function::Create(
        funcType, llvm::Function::ExternalLinkage, name, module.get());
    methods[name] = func;
}

// Generate LLVM struct types for all VSOP classes
void CodeGenerator::generateClassTypes() {
    const auto& class_defs = analyzer.getClassDefinitions();
    
    // First pass: create struct types (without body)
    for (const auto& [class_name, _] : class_defs) {
        if (class_name == "Object") continue; // Object already defined in includeRuntimeCode()
        
        if (class_name != "int32" && class_name != "bool" && 
            class_name != "string" && class_name != "unit") {
            class_types[class_name] = llvm::StructType::create(*context, class_name);
        }
    }
    
    // Second pass: define struct bodies with fields
    for (const auto& [class_name, class_def] : class_defs) {
        if (class_name == "Object" || class_name == "int32" || class_name == "bool" || 
            class_name == "string" || class_name == "unit") {
            continue;
        }
        
        std::vector<llvm::Type*> field_types;
        
        // First field is always a pointer to the parent class
        std::string parent_name = class_def.parent;
        if (parent_name.empty()) parent_name = "Object";
        
        field_types.push_back(llvm::PointerType::get(class_types[parent_name], 0));
        
        // Add fields from the class definition
        for (const auto& [field_name, field_type] : class_def.fields) {
            field_types.push_back(getLLVMType(field_type.toString()));
        }
        
        // Set the body of the struct type
        class_types[class_name]->setBody(field_types);
    }
}

void CodeGenerator::generateClassVTables() {
    const auto& class_defs = analyzer.getClassDefinitions();
    
    // Step 1: First collect all methods across the class hierarchy for each class
    std::unordered_map<std::string, std::vector<std::string>> class_vtable_methods;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> class_method_implementations;
    
    // Initialize with Object methods
    std::vector<std::string> object_methods = {
        "print", "printInt32", "printBool", "inputInt32", "inputString", "inputBool", "inputLine"
    };
    
    class_vtable_methods["Object"] = object_methods;
    for (const auto& method_name : object_methods) {
        class_method_implementations["Object"][method_name] = "Object__" + method_name;
    }
    
    // Determine vtable methods for all classes
    for (const auto& [class_name, class_def] : class_defs) {
        // Skip Object (already processed) and primitive types
        if (class_name == "Object" || class_name == "int32" || class_name == "bool" || 
            class_name == "string" || class_name == "unit") {
            continue;
        }
        
        // Start with parent's methods
        std::string parent_name = class_def.parent;
        if (parent_name.empty()) parent_name = "Object";
        
        // Inherit parent's vtable layout
        if (class_vtable_methods.find(parent_name) != class_vtable_methods.end()) {
            class_vtable_methods[class_name] = class_vtable_methods[parent_name];
            
            // Inherit parent's method implementations
            for (const auto& [method_name, impl_name] : class_method_implementations[parent_name]) {
                class_method_implementations[class_name][method_name] = impl_name;
            }
        } else {
            reportError("Parent class not found in vtable creation: " + parent_name);
            continue;
        }
        
        // Add/override methods from this class
        for (const auto& [method_name, method_sig] : class_def.methods) {
            // Check if this method is already in the vtable (override)
            bool found = false;
            for (size_t i = 0; i < class_vtable_methods[class_name].size(); ++i) {
                if (class_vtable_methods[class_name][i] == method_name) {
                    // Override the implementation
                    class_method_implementations[class_name][method_name] = class_name + "__" + method_name;
                    found = true;
                    break;
                }
            }
            
            // If not found, add it as a new virtual method
            if (!found) {
                class_vtable_methods[class_name].push_back(method_name);
                class_method_implementations[class_name][method_name] = class_name + "__" + method_name;
            }
        }
    }
    
    // Step 2: Create vtable type for each class
    std::unordered_map<std::string, llvm::StructType*> vtable_types;
    
    // Create Object vtable type if not already created
    if (!vtable_types.count("Object")) {
        std::string vtable_name = "Object_VTable";
        vtable_types["Object"] = llvm::StructType::create(*context, vtable_name);
        
        // Create the function pointer types for the vtable
        std::vector<llvm::Type*> vtable_elements;
        for (const auto& method_name : class_vtable_methods["Object"]) {
            // Get the function itself
            std::string func_name = "Object__" + method_name;
            llvm::Function* func = methods[func_name];
            
            if (!func) {
                reportError("Function not found for vtable: " + func_name);
                // Skip this method
                continue;
            }
            
            // Get the function type and create a function pointer
            llvm::FunctionType* func_type = func->getFunctionType();
            llvm::PointerType* func_ptr_type = llvm::PointerType::get(func_type, 0);
            vtable_elements.push_back(func_ptr_type);
        }
        
        // Set the body of the vtable type
        vtable_types["Object"]->setBody(vtable_elements);
    }
    
    // Create vtable types for all other classes
    for (const auto& [class_name, vtable_methods] : class_vtable_methods) {
        if (class_name == "Object" || class_name == "int32" || class_name == "bool" || 
            class_name == "string" || class_name == "unit") {
            continue;
        }
        
        std::string vtable_name = class_name + "_VTable";
        vtable_types[class_name] = llvm::StructType::create(*context, vtable_name);
        
        // Create the function pointer types for the vtable
        std::vector<llvm::Type*> vtable_elements;
        for (const auto& method_name : vtable_methods) {
            // Get the implementation class and function
            std::string impl_class_method = class_method_implementations[class_name][method_name];
            llvm::Function* func = methods[impl_class_method];
            
            if (!func) {
                reportError("Function not found for vtable: " + impl_class_method);
                // Create a dummy function pointer type to maintain vtable layout
                llvm::FunctionType* dummy_type = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(*context), {}, false);
                vtable_elements.push_back(llvm::PointerType::get(dummy_type, 0));
                continue;
            }
            
            // Get the function type and create a function pointer
            llvm::FunctionType* func_type = func->getFunctionType();
            llvm::PointerType* func_ptr_type = llvm::PointerType::get(func_type, 0);
            vtable_elements.push_back(func_ptr_type);
        }
        
        // Set the body of the vtable type
        vtable_types[class_name]->setBody(vtable_elements);
    }
    
    // Step 3: Create global vtable instances for each class
    std::unordered_map<std::string, llvm::GlobalVariable*> vtable_globals;
    
    // Create vtable instance for Object
    {
        std::string vtable_global_name = "Object_VTable_Instance";
        
        // Create the vtable constant
        std::vector<llvm::Constant*> vtable_elements;
        for (const auto& method_name : class_vtable_methods["Object"]) {
            std::string func_name = "Object__" + method_name;
            llvm::Function* func = methods[func_name];
            
            if (!func) {
                reportError("Function not found for vtable instance: " + func_name);
                // Use null pointer as fallback
                vtable_elements.push_back(llvm::ConstantPointerNull::get(
                    llvm::PointerType::get(llvm::Type::getVoidTy(*context), 0)));
                continue;
            }
            
            vtable_elements.push_back(func);
        }
        
        // Create the constant struct
        llvm::Constant* vtable_struct = llvm::ConstantStruct::get(
            vtable_types["Object"], vtable_elements);
        
        // Create the global variable
        llvm::GlobalVariable* vtable_global = new llvm::GlobalVariable(
            *module, vtable_types["Object"], true,
            llvm::GlobalValue::ExternalLinkage, vtable_struct, vtable_global_name);
        
        vtable_globals["Object"] = vtable_global;
    }
    
    // Create vtable instances for all other classes
    for (const auto& [class_name, vtable_methods] : class_vtable_methods) {
        if (class_name == "Object" || class_name == "int32" || class_name == "bool" || 
            class_name == "string" || class_name == "unit") {
            continue;
        }
        
        std::string vtable_global_name = class_name + "_VTable_Instance";
        
        // Create the vtable constant
        std::vector<llvm::Constant*> vtable_elements;
        for (const auto& method_name : vtable_methods) {
            // Get the implementation function
            std::string impl_class_method = class_method_implementations[class_name][method_name];
            llvm::Function* func = methods[impl_class_method];
            
            if (!func) {
                reportError("Function not found for vtable instance: " + impl_class_method);
                // Use null pointer as fallback
                vtable_elements.push_back(llvm::ConstantPointerNull::get(
                    llvm::PointerType::get(llvm::Type::getVoidTy(*context), 0)));
                continue;
            }
            
            vtable_elements.push_back(func);
        }
        
        // Create the constant struct
        llvm::Constant* vtable_struct = llvm::ConstantStruct::get(
            vtable_types[class_name], vtable_elements);
        
        // Create the global variable
        llvm::GlobalVariable* vtable_global = new llvm::GlobalVariable(
            *module, vtable_types[class_name], true,
            llvm::GlobalValue::ExternalLinkage, vtable_struct, vtable_global_name);
        
        vtable_globals[class_name] = vtable_global;
    }
    
    // Store vtable information for later use
    this->vtable_types = vtable_types;
    this->vtable_globals = vtable_globals;
    
    // Update class types to include vtable pointer
    for (const auto& [class_name, class_type] : class_types) {
        if (class_name == "int32" || class_name == "bool" || 
            class_name == "string" || class_name == "unit") {
            continue;
        }
        
        // Skip if vtable type not found
        if (vtable_types.find(class_name) == vtable_types.end()) {
            continue;
        }
        
        // Get existing body elements
        std::vector<llvm::Type*> body_elements;
        if (class_type->isStructTy() && !class_type->isOpaque()) {
            llvm::StructType* struct_type = llvm::cast<llvm::StructType>(class_type);
            for (unsigned i = 0; i < struct_type->getNumElements(); ++i) {
                body_elements.push_back(struct_type->getElementType(i));
            }
        }
        
        // If body is empty (opaque type), add the vtable pointer as first element
        if (body_elements.empty()) {
            body_elements.push_back(llvm::PointerType::get(vtable_types[class_name], 0));
            
            // Now add parent class fields and this class's fields
            std::string parent_name = class_defs.at(class_name).parent;
            if (parent_name.empty()) parent_name = "Object";
            
            // Add parent class fields - would need recursive traversal in a real implementation
            // This is simplified
            
            // Add this class's fields
            for (const auto& [field_name, field_type] : class_defs.at(class_name).fields) {
                body_elements.push_back(getLLVMType(field_type.toString()));
            }
            
            // Set the body of the class type
            llvm::cast<llvm::StructType>(class_type)->setBody(body_elements);
        }
    }
}

// Generate LLVM function declarations for all VSOP methods
void CodeGenerator::generateClassMethods() {
    const auto& class_defs = analyzer.getClassDefinitions();
    
    for (const auto& [class_name, class_def] : class_defs) {
        if (class_name == "int32" || class_name == "bool" || 
            class_name == "string" || class_name == "unit") {
            continue; // Skip primitive types
        }
        
        for (const auto& [method_name, method_sig] : class_def.methods) {
            // Skip methods from Object that we've already declared
            if (class_name == "Object" && 
                (method_name == "print" || method_name == "printInt32" || method_name == "inputInt32")) {
                continue;
            }
            
            // Create the method signature
            std::vector<llvm::Type*> param_types;
            
            // First parameter is always 'self' (pointer to the class)
            param_types.push_back(llvm::PointerType::get(class_types[class_name], 0));
            
            // Add the rest of the parameters
            for (const auto& param : method_sig.parameters) {
                param_types.push_back(getLLVMType(param.type.toString()));
            }
            
            // Get return type
            llvm::Type* return_type = getLLVMType(method_sig.returnType.toString());
            
            // Create the function type
            llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
            
            // Create the function
            std::string func_name = class_name + "__" + method_name;
            llvm::Function* func = llvm::Function::Create(
                func_type, llvm::Function::ExternalLinkage, func_name, module.get());
            
            // Set parameter names
            auto arg_it = func->arg_begin();
            arg_it->setName("self"); // First argument is always 'self'
            
            for (size_t i = 0; i < method_sig.parameters.size(); ++i) {
                ++arg_it;
                arg_it->setName(method_sig.parameters[i].name);
            }
            
            // Store the function in our methods map
            methods[func_name] = func;
        }
    }
}

// Generate the bodies of all methods
void CodeGenerator::generateMethodBodies() {
    const auto& class_defs = analyzer.getClassDefinitions();
    
    // For each class in the program
    for (const auto& cls : program->classes) {
        if (!cls) continue;
        
        current_class = cls->name;
        
        // Skip primitive types
        if (current_class == "int32" || current_class == "bool" || 
            current_class == "string" || current_class == "unit") {
            continue;
        }
        
        // Create a map of method names to AST method nodes for this class
        std::unordered_map<std::string, const Method*> ast_methods;
        for (const auto& method : cls->methods) {
            if (method) {
                ast_methods[method->name] = method.get();
            }
        }
        
        // For each method in the class definition
        const auto& class_def = class_defs.find(current_class);
        if (class_def == class_defs.end()) {
            reportError("Class definition not found for " + current_class);
            continue;
        }
        
        for (const auto& method_entry : class_def->second.methods) {
            std::string method_name = method_entry.first;
            std::string func_name = current_class + "__" + method_name;
            
            // Get the LLVM function
            current_function = methods[func_name];
            if (!current_function) {
                reportError("Function not found: " + func_name);
                continue;
            }
            
            // Find the method in the AST
            auto ast_method_it = ast_methods.find(method_name);
            if (ast_method_it == ast_methods.end()) {
                // This could be an inherited method that's not overridden, so skip it
                continue;
            }
            
            const Method* method = ast_method_it->second;
            
            // Create entry block
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", current_function);
            builder->SetInsertPoint(entry);
            
            // Clear the current variable map
            current_vars.clear();
            
            // Add function arguments to the current variable map
            auto arg_it = current_function->arg_begin();
            arg_it->setName("self"); // First argument is always 'self'
            
            // Store 'self' in current_vars
            current_vars["self"] = arg_it;
            
            // Add the rest of the parameters with their names
            for (size_t i = 0; i < method->formals.size(); ++i) {
                ++arg_it;
                if (i < method->formals.size() && method->formals[i]) {
                    arg_it->setName(method->formals[i]->name);
                    current_vars[method->formals[i]->name] = arg_it;
                }
            }
            
            // Generate code for the method body
            llvm::Value* body_val = nullptr;
            if (method->body) {
                body_val = generateExpression(method->body.get());
            }
            
            // Create return instruction
            if (current_function->getReturnType()->isVoidTy()) {
                // For unit return type
                builder->CreateRetVoid();
            } 
            else if (body_val) {
                // Return the computed value
                builder->CreateRet(body_val);
            } 
            else {
                // If body didn't generate a value (error or unit), return default
                llvm::Value* default_val = llvm::Constant::getNullValue(current_function->getReturnType());
                builder->CreateRet(default_val);
            }
        }
        
        current_function = nullptr;
    }
    
    current_class = "";
}

// Generate the main entry point
void CodeGenerator::generateMainEntryPoint() {
    // Check if Main class and main method exist
    if (!class_types.count("Main")) {
        reportError("Class Main not found");
        return;
    }
    
    std::string main_func_name = "Main__main";
    if (!methods.count(main_func_name)) {
        reportError("Method main not found in class Main");
        return;
    }
    
    // Create C-compatible main function
    llvm::FunctionType* main_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), false);
    llvm::Function* main_func = llvm::Function::Create(
        main_type, llvm::Function::ExternalLinkage, "main", module.get());
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", main_func);
    builder->SetInsertPoint(entry);
    
    // Create Main instance
    llvm::Function* main_ctor = methods["Main___new"];
    if (!main_ctor) {
        main_ctor = methods["Object___new"]; // Fallback
    }
    
    llvm::Value* main_instance = nullptr;
    if (main_ctor) {
        main_instance = builder->CreateCall(main_ctor);
    } else {
        // Fallback: allocate Main manually
        // 1. Get or declare malloc function
        llvm::Function* mallocFunc = module->getFunction("malloc");
        if (!mallocFunc) {
            llvm::FunctionType* mallocType = llvm::FunctionType::get(
                builder->getInt8PtrTy(),  
                {builder->getInt64Ty()},  
                false                     
            );
            mallocFunc = llvm::Function::Create(
                mallocType,
                llvm::Function::ExternalLinkage,
                "malloc",
                module.get()
            );
        }
        
        // 2. Calculate the size to allocate
        llvm::Value* allocSize = llvm::ConstantExpr::getSizeOf(class_types["Main"]);
        
        // 3. Create the call to malloc
        llvm::Value* main_alloc = builder->CreateCall(mallocFunc, {allocSize}, "main_instance");
        
        // 4. Cast the result to the appropriate type
        main_instance = builder->CreateBitCast(
            main_alloc, 
            llvm::PointerType::get(class_types["Main"], 0)
        );
    }
    
    // Call Main.main()
    llvm::Function* main_method = methods[main_func_name];
    llvm::Value* result = builder->CreateCall(main_method, {main_instance});
    
    // Return the result
    builder->CreateRet(result);
}

// Output the generated LLVM IR
void CodeGenerator::dumpIR(std::ostream& os) {
    std::string output;
    llvm::raw_string_ostream llvm_stream(output);
    module->print(llvm_stream, nullptr);
    os << output;
}

bool CodeGenerator::writeNativeExecutable(const std::string& output_file) {
    // First, output the LLVM IR to a temporary file
    std::string temp_ir_file = output_file + ".ll";
    std::error_code EC;
    llvm::raw_fd_ostream ir_out(temp_ir_file, EC);
    
    if (EC) {
        reportError("Could not open temporary file: " + EC.message());
        return false;
    }
    
    module->print(ir_out, nullptr);
    ir_out.close();
    
    // Following option 2 from the runtime README.md:
    // Compile the LLVM IR with the runtime object file
    std::string runtime_path = "runtime/runtime/object.c"; // Or .o if precompiled
    std::string compile_cmd = "clang -o " + output_file + " " + temp_ir_file + " " + runtime_path;
    
    int result = std::system(compile_cmd.c_str());
    if (result != 0) {
        reportError("Compilation failed with command: " + compile_cmd);
        return false;
    }
    
    // Clean up the temporary file
    std::remove(temp_ir_file.c_str());
    
    return true;
}

// Helper methods for code generation

// Report an error
void CodeGenerator::reportError(const std::string& message) {
    std::string error = source_file + ": code generation error: " + message;
    if (std::find(errors.begin(), errors.end(), error) == errors.end()) {
        errors.push_back(error);
    }
}

// Get LLVM type for a VSOP type
llvm::Type* CodeGenerator::getLLVMType(const std::string& vsop_type) {
    // Check primitive types first
    auto prim_it = primitive_types.find(vsop_type);
    if (prim_it != primitive_types.end()) {
        return prim_it->second;
    }
    
    // Then check class types
    auto class_it = class_types.find(vsop_type);
    if (class_it != class_types.end()) {
        return llvm::PointerType::get(class_it->second, 0);
    }
    
    // If not found, report error and return i8* as fallback
    reportError("Unknown type: " + vsop_type);
    return llvm::Type::getInt8PtrTy(*context);
}

// Create a string constant
llvm::Value* CodeGenerator::createStringConstant(const std::string& str) {
    // Add null terminator
    std::string with_null = str + '\0';
    
    // Create a constant array with the string data
    llvm::Constant* string_constant = llvm::ConstantDataArray::getString(*context, with_null);
    
    // Create a global variable to hold the string
    llvm::GlobalVariable* global_str = new llvm::GlobalVariable(
        *module, string_constant->getType(), true,
        llvm::GlobalValue::PrivateLinkage, string_constant, ".str");
    
    // Get a pointer to the first character
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0);
    llvm::Value* indices[] = {zero, zero};
    return builder->CreateInBoundsGEP(string_constant->getType(), global_str, indices, "str");
}

// Generate code for expressions

llvm::Value* CodeGenerator::generateExpression(const Expression* expr) {
    if (!expr) {
        reportError("Null expression");
        return nullptr;
    }
    
    if (const BinaryOp* binop = dynamic_cast<const BinaryOp*>(expr)) {
        return generateBinaryOp(binop);
    }
    else if (const UnaryOp* unop = dynamic_cast<const UnaryOp*>(expr)) {
        return generateUnaryOp(unop);
    }
    else if (const Call* call = dynamic_cast<const Call*>(expr)) {
        return generateCall(call);
    }
    else if (const New* newExpr = dynamic_cast<const New*>(expr)) {
        return generateNew(newExpr);
    }
    else if (const Let* letExpr = dynamic_cast<const Let*>(expr)) {
        return generateLet(letExpr);
    }
    else if (const If* ifExpr = dynamic_cast<const If*>(expr)) {
        return generateIf(ifExpr);
    }
    else if (const While* whileExpr = dynamic_cast<const While*>(expr)) {
        return generateWhile(whileExpr);
    }
    else if (const Assign* assignExpr = dynamic_cast<const Assign*>(expr)) {
        return generateAssign(assignExpr);
    }
    else if (const Block* blockExpr = dynamic_cast<const Block*>(expr)) {
        return generateBlock(blockExpr);
    }
    else if (const IntegerLiteral* intLit = dynamic_cast<const IntegerLiteral*>(expr)) {
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), intLit->value);
    }
    else if (const BooleanLiteral* boolLit = dynamic_cast<const BooleanLiteral*>(expr)) {
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), boolLit->value);
    }
    else if (const StringLiteral* strLit = dynamic_cast<const StringLiteral*>(expr)) {
        return createStringConstant(strLit->value);
    }
    else if (const UnitLiteral* unitLit = dynamic_cast<const UnitLiteral*>(expr)) {
        return nullptr; // unit has no value
    }
    else if (const Identifier* id = dynamic_cast<const Identifier*>(expr)) {
        return generateIdentifier(id);
    }
    else if (const Self* self = dynamic_cast<const Self*>(expr)) {
        return generateSelf(self);
    }
    
    reportError("Unknown expression type");
    return nullptr;
}

// Stub implementations for expression code generation
// These would need to be fully implemented

// Here's an example of how to implement the binary operation code generation
llvm::Value* CodeGenerator::generateBinaryOp(const BinaryOp* binop) {
    if (!binop) {
        reportError("Null binary operation expression");
        return nullptr;
    }
    
    // Generate code for left and right operands
    llvm::Value* left = generateExpression(binop->left.get());
    llvm::Value* right = generateExpression(binop->right.get());
    
    if (!left || !right) {
        return nullptr; // Error already reported
    }
    
    // Perform operation based on the operator
    if (binop->op == "+") {
        // Integer addition
        return builder->CreateAdd(left, right, "addtmp");
    }
    else if (binop->op == "-") {
        // Integer subtraction
        return builder->CreateSub(left, right, "subtmp");
    }
    else if (binop->op == "*") {
        // Integer multiplication
        return builder->CreateMul(left, right, "multmp");
    }
    else if (binop->op == "/") {
        // Integer division (signed)
        return builder->CreateSDiv(left, right, "divtmp");
    }
    else if (binop->op == "^") {
        // Power operation - implement using a loop or call to pow function
        // For simplicity, we'll create a call to a custom power function
        
        // First, check if we already created this function
        llvm::Function* pow_func = module->getFunction("vsop_pow");
        
        if (!pow_func) {
            // Create the power function for int32 if it doesn't exist
            llvm::Type* int32_type = llvm::Type::getInt32Ty(*context);
            std::vector<llvm::Type*> args_types = {int32_type, int32_type};
            llvm::FunctionType* func_type = llvm::FunctionType::get(int32_type, args_types, false);
            
            pow_func = llvm::Function::Create(func_type, llvm::Function::InternalLinkage, "vsop_pow", module.get());
            
            // Set argument names
            auto args_it = pow_func->arg_begin();
            args_it->setName("base");
            (++args_it)->setName("exp");
            
            // Create the function body
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", pow_func);
            llvm::BasicBlock* loop_check = llvm::BasicBlock::Create(*context, "loop_check", pow_func);
            llvm::BasicBlock* loop_body = llvm::BasicBlock::Create(*context, "loop_body", pow_func);
            llvm::BasicBlock* loop_exit = llvm::BasicBlock::Create(*context, "loop_exit", pow_func);
            
            // Get function arguments
            auto args = pow_func->arg_begin();
            llvm::Value* base = args++;
            llvm::Value* exp = args;
            
            // Create temporary builder for this function
            llvm::IRBuilder<> temp_builder(*context);
            
            // Entry block logic
            temp_builder.SetInsertPoint(entry);
            
            // Initialize result to 1 (base^0 = 1)
            llvm::Value* result = llvm::ConstantInt::get(int32_type, 1);
            llvm::Value* count = llvm::ConstantInt::get(int32_type, 0);
            
            // Create allocas for local variables
            llvm::AllocaInst* result_alloca = temp_builder.CreateAlloca(int32_type, nullptr, "result");
            llvm::AllocaInst* count_alloca = temp_builder.CreateAlloca(int32_type, nullptr, "count");
            
            // Initialize allocas
            temp_builder.CreateStore(result, result_alloca);
            temp_builder.CreateStore(count, count_alloca);
            
            // Branch to loop check
            temp_builder.CreateBr(loop_check);
            
            // Loop check block
            temp_builder.SetInsertPoint(loop_check);
            
            // Load current values
            llvm::Value* current_count = temp_builder.CreateLoad(int32_type, count_alloca, "current_count");
            llvm::Value* current_result = temp_builder.CreateLoad(int32_type, result_alloca, "current_result");
            
            // Check if count < exp
            llvm::Value* cond = temp_builder.CreateICmpSLT(current_count, exp, "count_lt_exp");
            temp_builder.CreateCondBr(cond, loop_body, loop_exit);
            
            // Loop body block
            temp_builder.SetInsertPoint(loop_body);
            
            // result = result * base
            llvm::Value* new_result = temp_builder.CreateMul(current_result, base, "new_result");
            temp_builder.CreateStore(new_result, result_alloca);
            
            // count = count + 1
            llvm::Value* new_count = temp_builder.CreateAdd(current_count, llvm::ConstantInt::get(int32_type, 1), "new_count");
            temp_builder.CreateStore(new_count, count_alloca);
            
            // Branch back to loop check
            temp_builder.CreateBr(loop_check);
            
            // Loop exit block
            temp_builder.SetInsertPoint(loop_exit);
            
            // Return the result
            temp_builder.CreateRet(current_result);
        }
        
        // Call the power function
        return builder->CreateCall(pow_func, {left, right}, "powtmp");
    }
    else if (binop->op == "=") {
        // Equality comparison - result is a boolean (i1)
        if (left->getType()->isIntegerTy(32)) {
            // For integers
            return builder->CreateICmpEQ(left, right, "eqtmp");
        }
        else if (left->getType()->isIntegerTy(1)) {
            // For booleans
            return builder->CreateICmpEQ(left, right, "eqtmp");
        }
        else if (left->getType()->isPointerTy()) {
            // For objects/strings
            return builder->CreateICmpEQ(left, right, "eqtmp");
        }
        reportError("Unsupported types for equality comparison");
        return nullptr;
    }
    else if (binop->op == "<") {
        // Less than comparison - result is a boolean (i1)
        return builder->CreateICmpSLT(left, right, "lttmp");
    }
    else if (binop->op == "<=") {
        // Less than or equal comparison - result is a boolean (i1)
        return builder->CreateICmpSLE(left, right, "letmp");
    }
    else if (binop->op == "and") {
        // Logical AND - result is a boolean (i1)
        return builder->CreateAnd(left, right, "andtmp");
    }
    
    reportError("Unknown binary operator: " + binop->op);
    return nullptr;
}

// Let's also implement a simpler expression handler: literals
llvm::Value* CodeGenerator::generateLiteral(const Literal* literal) {
    if (const IntegerLiteral* intLit = dynamic_cast<const IntegerLiteral*>(literal)) {
        // Create an i32 constant for integer literals
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), intLit->value, true);
    }
    else if (const BooleanLiteral* boolLit = dynamic_cast<const BooleanLiteral*>(literal)) {
        // Create an i1 constant for boolean literals
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), boolLit->value, false);
    }
    else if (const StringLiteral* strLit = dynamic_cast<const StringLiteral*>(literal)) {
        // Create a string constant (global constant array with null terminator)
        return createStringConstant(strLit->value);
    }
    else if (dynamic_cast<const UnitLiteral*>(literal)) {
        // Unit literal doesn't have a value - return null
        return nullptr;
    }
    
    reportError("Unknown literal type");
    return nullptr;
}

// Implementation for unary operations
llvm::Value* CodeGenerator::generateUnaryOp(const UnaryOp* unop) {
    if (!unop) {
        reportError("Null unary operation expression");
        return nullptr;
    }
    
    // Generate code for the operand
    llvm::Value* operand = generateExpression(unop->expr.get());
    
    if (!operand) {
        return nullptr; // Error already reported
    }
    
    // Perform operation based on the operator
    if (unop->op == "-") {
        // Unary minus - negate the operand
        return builder->CreateNeg(operand, "negtmp");
    }
    else if (unop->op == "not") {
        // Logical NOT - invert the boolean value
        return builder->CreateNot(operand, "nottmp");
    }
    else if (unop->op == "isnull") {
        // Check if the object is null
        llvm::Value* null_ptr = llvm::ConstantPointerNull::get(
            llvm::cast<llvm::PointerType>(operand->getType()));
        return builder->CreateICmpEQ(operand, null_ptr, "isnulltmp");
    }
    
    reportError("Unknown unary operator: " + unop->op);
    return nullptr;
}

// Implementation for if expressions
llvm::Value* CodeGenerator::generateIf(const If* ifExpr) {
    if (!ifExpr) {
        reportError("Null if expression");
        return nullptr;
    }
    
    // Generate code for the condition
    llvm::Value* condition = generateExpression(ifExpr->condition.get());
    
    if (!condition) {
        return nullptr; // Error already reported
    }
    
    // Create basic blocks for then, else, and merge
    llvm::Function* func = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*context, "then", func);
    llvm::BasicBlock* else_bb = nullptr;
    llvm::BasicBlock* merge_bb = nullptr;
    
    if (ifExpr->else_expr) {
        else_bb = llvm::BasicBlock::Create(*context, "else");
        merge_bb = llvm::BasicBlock::Create(*context, "ifcont");
    } else {
        // No else branch, so merge block is the same as else block
        merge_bb = llvm::BasicBlock::Create(*context, "ifcont");
    }
    
    // Create conditional branch based on condition
    builder->CreateCondBr(condition, then_bb, else_bb ? else_bb : merge_bb);
    
    // Generate code for then branch
    builder->SetInsertPoint(then_bb);
    llvm::Value* then_val = generateExpression(ifExpr->then_expr.get());
    if (!then_val) {
        return nullptr; // Error already reported
    }
    
    // Branch to merge block
    builder->CreateBr(merge_bb);
    
    // Remember the block where we generated the 'then' value for PHI node
    then_bb = builder->GetInsertBlock();
    
    // Generate code for else branch if it exists
    llvm::Value* else_val = nullptr;
    if (ifExpr->else_expr) {
        func->getBasicBlockList().push_back(else_bb);
        builder->SetInsertPoint(else_bb);
        
        else_val = generateExpression(ifExpr->else_expr.get());
        if (!else_val) {
            return nullptr; // Error already reported
        }
        
        // Branch to merge block
        builder->CreateBr(merge_bb);
        
        // Remember the block where we generated the 'else' value for PHI node
        else_bb = builder->GetInsertBlock();
    }
    
    // Generate code for merge block
    func->getBasicBlockList().push_back(merge_bb);
    builder->SetInsertPoint(merge_bb);
    
    // Create PHI node for the result if needed
    if (ifExpr->else_expr) {
        llvm::PHINode* phi = builder->CreatePHI(then_val->getType(), 2, "iftmp");
        
        phi->addIncoming(then_val, then_bb);
        phi->addIncoming(else_val, else_bb);
        
        return phi;
    } else {
        // If no else branch, the result is unit (void)
        return nullptr;
    }
}

// Implementation for identifiers (variable access)
llvm::Value* CodeGenerator::generateIdentifier(const Identifier* id) {
    if (!id) {
        reportError("Null identifier expression");
        return nullptr;
    }
    
    // Check if it's a local variable
    auto it = current_vars.find(id->name);
    if (it != current_vars.end()) {
        return it->second;
    }
    
    // Check if it's a field of the current class
    if (!current_class.empty()) {
        // Get the 'self' parameter
        llvm::Function* func = builder->GetInsertBlock()->getParent();
        llvm::Argument* self = func->arg_begin();
        
        // Look up field index
        std::optional<Type> field_type_opt = analyzer.findFieldType(current_class, id->name);
        if (field_type_opt.has_value()) {
            // Calculate field index - this is simplistic and assumes fields are in order
            // In a real implementation, you'd need a proper field index map
            int field_idx = 1; // Skip the vtable pointer
            
            // Create a GEP to access the field
            llvm::Value* field_ptr = builder->CreateStructGEP(
                class_types[current_class], self, field_idx, id->name);
            
            // Load the field value
            return builder->CreateLoad(getLLVMType(field_type_opt.value().toString()), field_ptr, id->name);
        }
    }
    
    reportError("Undefined identifier: " + id->name);
    return nullptr;
}

// Implementation for self reference
llvm::Value* CodeGenerator::generateSelf(const Self* self) {
    if (!self) {
        reportError("Null self expression");
        return nullptr;
    }
    
    if (current_class.empty()) {
        reportError("'self' used outside of a class method");
        return nullptr;
    }
    
    // Get the 'self' parameter from the current function
    llvm::Function* func = builder->GetInsertBlock()->getParent();
    llvm::Argument* self_arg = func->arg_begin();
    
    return self_arg;
}

// Implementation for method calls
llvm::Value* CodeGenerator::generateCall(const Call* call) {
    if (!call) {
        reportError("Null call expression");
        return nullptr;
    }
    
    // Generate code for the object expression (or use 'self' if null)
    llvm::Value* object = nullptr;
    std::string object_class_name;
    
    if (call->object) {
        object = generateExpression(call->object.get());
        
        if (!object) {
            return nullptr; // Error already reported
        }
        
        // Determine the class of the object
        if (const Self* self = dynamic_cast<const Self*>(call->object.get())) {
            object_class_name = current_class;
        }
        else if (const New* newExpr = dynamic_cast<const New*>(call->object.get())) {
            object_class_name = newExpr->type_name;
        }
        else {
            // For other expressions, we need to determine the class at runtime
            // This is a simplification - in a real implementation, you'd need RTTI or virtual methods
            object_class_name = "Object"; // Use Object as fallback
        }
    }
    else {
        // Implicit self
        if (current_class.empty()) {
            reportError("Method call without object outside of a class method");
            return nullptr;
        }
        
        // Get the 'self' parameter from the current function
        llvm::Function* func = builder->GetInsertBlock()->getParent();
        object = func->arg_begin();
        object_class_name = current_class;
    }
    
    // Find the method in the class hierarchy
    std::optional<MethodSignature> method_sig_opt = analyzer.findMethodSignature(object_class_name, call->method_name);
    
    if (!method_sig_opt.has_value()) {
        // Check if it's a built-in Object method
        if (call->method_name == "print" || call->method_name == "printInt32" || 
            call->method_name == "inputInt32" || call->method_name == "inputString") {
            // These are handled specially as they're part of the runtime
            std::string method_name = "Object__" + call->method_name;
            llvm::Function* method_func = methods[method_name];
            
            if (!method_func) {
                reportError("Built-in method not found: " + method_name);
                return nullptr;
            }
            
            // Prepare arguments
            std::vector<llvm::Value*> args;
            args.push_back(object); // First argument is always the object
            
            // Add method arguments
            for (const auto& arg : call->arguments) {
                llvm::Value* arg_val = generateExpression(arg.get());
                if (!arg_val) {
                    return nullptr; // Error already reported
                }
                args.push_back(arg_val);
            }
            
            // Call the method
            return builder->CreateCall(method_func, args, call->method_name + "_call");
        }
        
        reportError("Method not found: " + call->method_name + " in class " + object_class_name);
        return nullptr;
    }
    
    // Construct the method name
    std::string method_name = object_class_name + "__" + call->method_name;
    
    // Find the method function
    llvm::Function* method_func = methods[method_name];
    
    if (!method_func) {
        reportError("Method function not found: " + method_name);
        return nullptr;
    }
    
    // Prepare arguments
    std::vector<llvm::Value*> args;
    args.push_back(object); // First argument is always the object
    
    // Check argument count
    if (call->arguments.size() != method_sig_opt.value().parameters.size()) {
        reportError("Incorrect number of arguments for method " + call->method_name + 
                    ": expected " + std::to_string(method_sig_opt.value().parameters.size()) + 
                    ", got " + std::to_string(call->arguments.size()));
        return nullptr;
    }
    
    // Add method arguments
    for (size_t i = 0; i < call->arguments.size(); i++) {
        llvm::Value* arg_val = generateExpression(call->arguments[i].get());
        if (!arg_val) {
            return nullptr; // Error already reported
        }
        args.push_back(arg_val);
    }
    
    // Call the method
    return builder->CreateCall(method_func, args, call->method_name + "_call");
}

// Implementation for blocks
llvm::Value* CodeGenerator::generateBlock(const Block* block) {
    if (!block) {
        reportError("Null block expression");
        return nullptr;
    }
    
    // If the block is empty, return nullptr (unit value)
    if (block->expressions.empty()) {
        return nullptr;
    }
    
    // Generate code for each expression in the block
    llvm::Value* result = nullptr;
    for (const auto& expr : block->expressions) {
        // Generate code for this expression
        result = generateExpression(expr.get());
        
        if (!result && expr != block->expressions.back()) {
            // Only the last expression can return unit (nullptr)
            // For intermediate expressions, we should at least have generated code
            reportError("Failed to generate code for block expression");
            return nullptr;
        }
    }
    
    // Return the value of the last expression
    return result;
}

// Implementation for assignments
llvm::Value* CodeGenerator::generateAssign(const Assign* assign) {
    if (!assign) {
        reportError("Null assignment expression");
        return nullptr;
    }
    
    // Generate code for the right-hand side expression
    llvm::Value* value = generateExpression(assign->expr.get());
    
    if (!value) {
        return nullptr; // Error already reported
    }
    
    // Check if it's a local variable
    auto it = current_vars.find(assign->name);
    if (it != current_vars.end()) {
        // For simplicity, we'll just update the variable map
        // In a real implementation with proper scoping, you'd store variables in alloca and update with store
        current_vars[assign->name] = value;
        return value;
    }
    
    // Check if it's a field of the current class
    if (!current_class.empty()) {
        // Get the 'self' parameter
        llvm::Function* func = builder->GetInsertBlock()->getParent();
        llvm::Argument* self = func->arg_begin();
        
        // Look up field index
        std::optional<Type> field_type_opt = analyzer.findFieldType(current_class, assign->name);
        if (field_type_opt.has_value()) {
            // Calculate field index - this is simplistic and assumes fields are in order
            // In a real implementation, you'd need a proper field index map
            int field_idx = 1; // Skip the vtable pointer
            
            // Create a GEP to access the field
            llvm::Value* field_ptr = builder->CreateStructGEP(
                class_types[current_class], self, field_idx, assign->name);
            
            // Store the new value
            builder->CreateStore(value, field_ptr);
            return value;
        }
    }
    
    reportError("Undefined variable or field for assignment: " + assign->name);
    return nullptr;
}

// Implementation for let expressions
llvm::Value* CodeGenerator::generateLet(const Let* letExpr) {
    if (!letExpr) {
        reportError("Null let expression");
        return nullptr;
    }
    
    // Generate code for initializer if present
    llvm::Value* init_val = nullptr;
    if (letExpr->init_expr) {
        init_val = generateExpression(letExpr->init_expr.get());
        
        if (!init_val) {
            return nullptr; // Error already reported
        }
    }
    else {
        // Default initialization based on type
        if (letExpr->type == "int32") {
            init_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0);
        }
        else if (letExpr->type == "bool") {
            init_val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), 0);
        }
        else if (letExpr->type == "string") {
            init_val = createStringConstant("");
        }
        else {
            // Default to null for class types
            llvm::Type* type_val = getLLVMType(letExpr->type);
            init_val = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type_val));
        }
    }
    
    // Add variable to current scope
    // In a real implementation with proper scoping, you'd create an alloca and store the value
    current_vars[letExpr->name] = init_val;
    
    // Generate code for the scope expression
    llvm::Value* scope_val = generateExpression(letExpr->scope_expr.get());
    
    // Remove variable from current scope
    current_vars.erase(letExpr->name);
    
    return scope_val;
}

// Implementation for while loops
llvm::Value* CodeGenerator::generateWhile(const While* whileExpr) {
    if (!whileExpr) {
        reportError("Null while expression");
        return nullptr;
    }
    
    // Create basic blocks for the loop
    llvm::Function* func = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*context, "while.cond", func);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context, "while.body");
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*context, "while.end");
    
    // Branch to condition block
    builder->CreateBr(cond_bb);
    
    // Generate condition code
    builder->SetInsertPoint(cond_bb);
    llvm::Value* cond_val = generateExpression(whileExpr->condition.get());
    
    if (!cond_val) {
        return nullptr; // Error already reported
    }
    
    // Create conditional branch
    builder->CreateCondBr(cond_val, body_bb, end_bb);
    
    // Generate body code
    func->getBasicBlockList().push_back(body_bb);
    builder->SetInsertPoint(body_bb);
    
    llvm::Value* body_val = generateExpression(whileExpr->body.get());
    
    if (!body_val && !dynamic_cast<const UnitLiteral*>(whileExpr->body.get())) {
        // Only UnitLiteral can return nullptr legitimately
        reportError("Failed to generate code for while body");
        return nullptr;
    }
    
    // Loop back to condition
    builder->CreateBr(cond_bb);
    
    // Set insertion point to end block
    func->getBasicBlockList().push_back(end_bb);
    builder->SetInsertPoint(end_bb);
    
    // While expression always returns unit (void)
    return nullptr;
}

// Implementation for new expressions
llvm::Value* CodeGenerator::generateNew(const New* newExpr) {
    if (!newExpr) {
        reportError("Null new expression");
        return nullptr;
    }
    
    // Check if the class exists
    auto it = class_types.find(newExpr->type_name);
    if (it == class_types.end()) {
        reportError("Unknown class type: " + newExpr->type_name);
        return nullptr;
    }
    
    // Get the class structure type
    llvm::StructType* class_type = it->second;
    
    // Call the constructor function
    std::string ctor_name = newExpr->type_name + "___new";
    llvm::Function* ctor_func = module->getFunction(ctor_name);
    
    if (!ctor_func) {
        // If constructor not found, try to create a simple one
        
        // First create the function type (no arguments, returns class pointer)
        llvm::FunctionType* func_type = llvm::FunctionType::get(
            llvm::PointerType::get(class_type, 0), false);
        
        // Create the function
        ctor_func = llvm::Function::Create(
            func_type, llvm::Function::ExternalLinkage, ctor_name, module.get());
        
        // Create the function body
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", ctor_func);
        llvm::IRBuilder<> temp_builder(*context);
        temp_builder.SetInsertPoint(entry);
        
        // Allocate memory for the object
        llvm::Value* size_val = llvm::ConstantExpr::getSizeOf(class_type);
        llvm::Value* mem = temp_builder.CreateCall(
            module->getOrInsertFunction("malloc", 
                llvm::Type::getInt8PtrTy(*context),
                llvm::Type::getInt64Ty(*context)),
            {size_val}, "mem");
        
        // Cast to class pointer
        llvm::Value* obj_ptr = temp_builder.CreateBitCast(
            mem, llvm::PointerType::get(class_type, 0), "obj_ptr");
        
        // Set vtable pointer (first field)
        // Note: In a real implementation, you need to set up the proper vtable
        llvm::Value* null_vtable = llvm::ConstantPointerNull::get(
            llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0));
        llvm::Value* vtable_ptr = temp_builder.CreateStructGEP(class_type, obj_ptr, 0, "vtable_ptr");
        temp_builder.CreateStore(null_vtable, vtable_ptr);
        
        // Return the object pointer
        temp_builder.CreateRet(obj_ptr);
    }
    
    // Call the constructor
    return builder->CreateCall(ctor_func);
}


} // namespace VSOP