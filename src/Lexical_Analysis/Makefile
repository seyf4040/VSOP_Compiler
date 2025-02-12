CXX             = clang++
CXXFLAGS        = -Wall -Wextra

BISONFLAGS      = -d

EXEC            = vsopc

SRC             = main.cpp \
                  driver.cpp \
                  parser.cpp \
                  lexer.cpp \
                  utils.cpp

OBJ             = $(SRC:.cpp=.o)

all: $(EXEC)

main.o: driver.hpp parser.hpp utils.hpp

driver.o: driver.hpp parser.hpp utils.hpp

parser.o: driver.hpp parser.hpp

lexer.o: driver.hpp parser.hpp

utils.o: utils.hpp

$(EXEC): $(OBJ)
	$(CXX) -o $@ $(LDFLAGS) $(OBJ)

parser.cpp: parser.y
	bison $(BISONFLAGS) -o parser.cpp $^

parser.hpp: parser.y
	bison $(BISONFLAGS) -o parser.cpp $^

lexer.cpp: lexer.lex
	flex $(LEXFLAGS) -o lexer.cpp $^

clean:
	@rm -f $(EXEC)
	@rm -f $(OBJ)
	@rm -f lexer.cpp
	@rm -f parser.cpp parser.hpp location.hh

.PHONY: clean
