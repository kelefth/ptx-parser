all: lexer.o parser.o InstrStatement.o Operand.o
	g++ -g lexer.o parser.o InstrStatement.o Operand.o -o ptx-parser -std=c++17

lexer.o: lexer/lexer.h
	g++ -g -c lexer/lexer.cpp -std=c++17

parser.o: parser/parser.h lexer/lexer.h InstrStatement.h Operand.h
	g++ -g -c parser/parser.cpp -std=c++17

InstrStatement.o: InstrStatement.h Statement.h Operand.h
	g++ -g -c InstrStatement.cpp -std=c++17

Operand.o: Operand.h
	g++ -g -c Operand.cpp -std=c++17

clean:
	rm *.o ptx-parser