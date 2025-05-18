CXX             = clang++
CXXFLAGS        = -Wall -Wextra -std=c++17 -I/usr/include/llvm-14
LLVM_LIBS       = -lLLVM
LDFLAGS         = $(LLVM_LIBS)
EXEC            = vsopc
SRC             = main.cpp \
                  driver.cpp \
                  parser.cpp \
                  lexer.cpp \
                  utils.cpp \
                  AST.cpp \
                  PrettyPrinter.cpp \
                  SemanticAnalyzer.cpp \
                  TypeChecker.cpp \
                  SemanticChecker.cpp \
                  CodeGenerator.cpp

OBJ             = $(SRC:.cpp=.o)
RUNTIME_DIR     = runtime/runtime
RUNTIME_SRC     = $(RUNTIME_DIR)/object.c
RUNTIME_OBJ     = $(RUNTIME_DIR)/object.o

all: $(EXEC) $(RUNTIME_OBJ)

main.o: driver.hpp parser.hpp utils.hpp AST.hpp SemanticChecker.hpp CodeGenerator.hpp
driver.o: driver.hpp parser.hpp utils.hpp AST.hpp PrettyPrinter.hpp SemanticChecker.hpp CodeGenerator.hpp
parser.o: driver.hpp parser.hpp AST.hpp
lexer.o: driver.hpp parser.hpp utils.hpp
utils.o: utils.hpp
AST.o: AST.hpp
PrettyPrinter.o: PrettyPrinter.hpp AST.hpp
SemanticAnalyzer.o: SemanticAnalyzer.hpp AST.hpp
TypeChecker.o: TypeChecker.hpp SemanticAnalyzer.hpp AST.hpp
SemanticChecker.o: SemanticChecker.hpp TypeChecker.hpp SemanticAnalyzer.hpp AST.hpp
CodeGenerator.o: CodeGenerator.hpp AST.hpp SemanticAnalyzer.hpp

$(EXEC): $(OBJ)
	$(CXX) -o $@ $(LDFLAGS) $(OBJ)

parser.cpp parser.hpp: parser.y
	bison $(BISONFLAGS) -o parser.cpp $^

lexer.cpp: lexer.lex
	flex $(LEXFLAGS) -o lexer.cpp $^

# Prepare the runtime object file
$(RUNTIME_OBJ): $(RUNTIME_SRC)
	@mkdir -p $(dir $@)
	clang -c $< -o $@

install-tools:
	@which flex > /dev/null || (echo "Installing flex..." && sudo apt-get install -y flex)
	@which bison > /dev/null || (echo "Installing bison..." && sudo apt-get install -y bison)
	@which llvm-config > /dev/null || (echo "Installing LLVM..." && sudo apt-get install -y llvm-14-dev)
	@which clang > /dev/null || (echo "Installing clang..." && sudo apt-get install -y clang)

clean:
	@rm -f $(EXEC)
	@rm -f $(OBJ)
	@rm -f lexer.cpp
	@rm -f parser.cpp parser.hpp location.hh position.hh stack.hh
	@rm -f $(RUNTIME_OBJ)
	@rm -f *.ll *.o

# Full installation
install: install-tools $(RUNTIME_OBJ)

# Compile a VSOP file to executable (useful for testing)
%.o: %.ll
	clang -c $< -o $@

%: %.o $(RUNTIME_OBJ)
	clang $^ -o $@

# Run test on sample program
test: $(EXEC) $(RUNTIME_OBJ)
	@echo "Running lexical analysis test..."
	@./$(EXEC) -l test.vsop
	@echo "Running syntax analysis test..."
	@./$(EXEC) -p test.vsop
	@echo "Running semantic analysis test..."
	@./$(EXEC) -c test.vsop
	@echo "Running LLVM IR generation test..."
	@./$(EXEC) -i test.vsop > test.vsop.ll
	@echo "Running executable generation test..."
	@./$(EXEC) test.vsop
	@echo "Running generated executable..."
	@./test

.PHONY: clean install-tools install test