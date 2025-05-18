CXX             = clang++
LLVM_CONFIG      = llvm-config
CXXFLAGS        = -Wall -Wextra -std=c++11 $(shell ${LLVM_CONFIG} --cxxflags) -fexceptions
LDFLAGS         = $(shell ${LLVM_CONFIG} --ldflags --system-libs --libs core support irreader bitwriter linker all) -fexceptions
BISONFLAGS      = -d
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

RUNTIME_SRC     = runtime.c

OBJ             = $(SRC:.cpp=.o)
RUNTIME_OBJ     = $(RUNTIME_SRC:.c=.o)

all: $(EXEC) runtime.o

main.o: driver.hpp parser.hpp utils.hpp AST.hpp SemanticChecker.hpp CodeGenerator.hpp
driver.o: driver.hpp parser.hpp utils.hpp AST.hpp PrettyPrinter.hpp
parser.o: driver.hpp parser.hpp AST.hpp
lexer.o: driver.hpp parser.hpp utils.hpp
utils.o: utils.hpp
AST.o: AST.hpp
PrettyPrinter.o: PrettyPrinter.hpp AST.hpp
SemanticAnalyzer.o: SemanticAnalyzer.hpp AST.hpp
TypeChecker.o: TypeChecker.hpp SemanticAnalyzer.hpp AST.hpp
SemanticChecker.o: SemanticChecker.hpp TypeChecker.hpp SemanticAnalyzer.hpp AST.hpp
CodeGenerator.o: CodeGenerator.hpp AST.hpp SemanticAnalyzer.hpp TypeChecker.hpp

runtime.o: runtime.c
	$(CC) -c $< -o $@

$(EXEC): $(OBJ)
	$(CXX) -o $@ $(LDFLAGS) $(OBJ)

parser.cpp parser.hpp: parser.y
	bison $(BISONFLAGS) -o parser.cpp $^

lexer.cpp: lexer.lex
	flex $(LEXFLAGS) -o lexer.cpp $^

install-tools:
	@which flex > /dev/null || (echo "Installing flex..." && sudo apt-get install -y flex)
	@which bison > /dev/null || (echo "Installing bison..." && sudo apt-get install -y bison)
	@which llvm-config > /dev/null || (echo "Installing LLVM..." && sudo apt-get install -y llvm llvm-dev)
	@which clang > /dev/null || (echo "Installing clang..." && sudo apt-get install -y clang)

clean:
	@rm -f $(EXEC) $(OBJ) $(RUNTIME_OBJ)
	@rm -f lexer.cpp
	@rm -f parser.cpp parser.hpp location.hh position.hh stack.hh
	@rm -f *.ll *.o

test: $(EXEC)
	@echo "Running lexical analysis test..."
	@./$(EXEC) -l test.vsop
	@echo "Running syntax analysis test..."
	@./$(EXEC) -p test.vsop
	@echo "Running semantic analysis test..."
	@./$(EXEC) -c test.vsop
	@echo "Running LLVM IR generation test..."
	@./$(EXEC) -i test.vsop
	@echo "Running full compilation test..."
	@./$(EXEC) test.vsop
	@./test

.PHONY: clean install-tools test