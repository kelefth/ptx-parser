all: lexer.o parser.o InstrStatement.o DirectStatement.o Operand.o AddressExpr.o ModuleDirectStatement.o ParamDirectStatement.o KernelDirectStatement.o
	g++ -g lexer.o parser.o InstrStatement.o DirectStatement.o Operand.o AddressExpr.o ModuleDirectStatement.o ParamDirectStatement.o KernelDirectStatement.o -o ptx-parser -std=c++17

lexer.o: lexer/lexer.h
	g++ -g -c lexer/lexer.cpp -std=c++17

parser.o: parser/parser.h lexer/lexer.h InstrStatement.h DirectStatement.h Operand.h AddressExpr.h
	g++ -g -c parser/parser.cpp -std=c++17

InstrStatement.o: InstrStatement.h Statement.h Operand.h
	g++ -g -c InstrStatement.cpp -std=c++17

DirectStatement.o: DirectStatement.h Statement.h
	g++ -g -c DirectStatement.cpp -std=c++17

ModuleDirectStatement.o: DirectStatement.h
	g++ -g -c ModuleDirectStatement.cpp -std=c++17

ParamDirectStatement.o: DirectStatement.h
	g++ -g -c ParamDirectStatement.cpp -std=c++17

KernelDirectStatement.o: Statement.h DirectStatement.h ParamDirectStatement.h
	g++ -g -c KernelDirectStatement.cpp -std=c++17

Operand.o: Operand.h AddressExpr.h
	g++ -g -c Operand.cpp -std=c++17

AddressExpr.o: AddressExpr.h Operand.h
	g++ -g -c AddressExpr.cpp -std=c++17

clean:
	rm *.o ptx-parser