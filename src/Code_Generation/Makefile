CXX             = clang++
CXXFLAGS        = -Wall -Wextra -std=c++17
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
                  SemanticChecker.cpp

OBJ             = $(SRC:.cpp=.o)

all: $(EXEC)

main.o: driver.hpp parser.hpp utils.hpp AST.hpp SemanticChecker.hpp
driver.o: driver.hpp parser.hpp utils.hpp AST.hpp PrettyPrinter.hpp
parser.o: driver.hpp parser.hpp AST.hpp
lexer.o: driver.hpp parser.hpp utils.hpp
utils.o: utils.hpp
AST.o: AST.hpp
PrettyPrinter.o: PrettyPrinter.hpp AST.hpp
SemanticAnalyzer.o: SemanticAnalyzer.hpp AST.hpp
TypeChecker.o: TypeChecker.hpp SemanticAnalyzer.hpp AST.hpp
SemanticChecker.o: SemanticChecker.hpp TypeChecker.hpp SemanticAnalyzer.hpp AST.hpp

$(EXEC): $(OBJ)
	$(CXX) -o $@ $(LDFLAGS) $(OBJ)

parser.cpp parser.hpp: parser.y
	bison $(BISONFLAGS) -o parser.cpp $^

lexer.cpp: lexer.lex
	flex $(LEXFLAGS) -o lexer.cpp $^

install-tools:
	@which flex > /dev/null || (echo "Installing flex..." && sudo apt-get install -y flex)
	@which bison > /dev/null || (echo "Installing bison..." && sudo apt-get install -y bison)

clean:
	@rm -f $(EXEC)
	@rm -f $(OBJ)
	@rm -f lexer.cpp
	@rm -f parser.cpp parser.hpp location.hh position.hh stack.hh

test: $(EXEC)
	@echo "Running lexical analysis test..."
	@./$(EXEC) -l test.vsop
	@echo "Running syntax analysis test..."
	@./$(EXEC) -p test.vsop
	@echo "Running semantic analysis test..."
	@./$(EXEC) -c test.vsop

.PHONY: clean install-tools test