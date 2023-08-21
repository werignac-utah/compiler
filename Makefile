FLAGS=
TEST=test.jpl

CXX=clang++
CXXFLAGS=-Og -std=c++17 -Werror -Wall -fsanitize=address,undefined -fno-sanitize-recover=address,undefined

LEXER=./lexer/

all: run

compile: compiler.o

compiler.o: compiler.cpp
	$(CXX) $(CXXFLAGS) -c compiler.cpp -ILEXER -o compiler.o

a.out: compiler.o
	$(CXX) $(CXXFLAGS) compiler.o -o a.out

run: a.out
	./a.out $(TEST) $(FLAGS)

clean:
	rm -f *.o a.out
