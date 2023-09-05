all: ArithmeticInst.o SextInst.o AllocaInst.o LoadInst.o StoreInst.o CallInst.o GetElementPtrInst.o IcmpInst.o BrInst.o RetInst.o LLvmInstruction.o
	g++ -g *.o -o llvm-out -std=c++17

ArithmeticInst.o: BinaryInsts/ArithmeticInst.h
	g++ -g -c BinaryInsts/ArithmeticInst.cpp -std=c++17

SextInst.o: ConversionInsts/SextInst.h
	g++ -g -c ConversionInsts/SextInst.cpp -std=c++17

AllocaInst.o: MemoryInsts/AllocaInst.h
	g++ -g -c MemoryInsts/AllocaInst.cpp -std=c++17

LoadInst.o: MemoryInsts/LoadInst.h
	g++ -g -c MemoryInsts/LoadInst.cpp -std=c++17

StoreInst.o: MemoryInsts/StoreInst.h
	g++ -g -c MemoryInsts/StoreInst.cpp -std=c++17

CallInst.o: OtherInsts/CallInst.h
	g++ -g -c OtherInsts/CallInst.cpp -std=c++17

GetElementPtrInst.o: OtherInsts/GetElementPtrInst.h
	g++ -g -c OtherInsts/GetElementPtrInst.cpp -std=c++17

IcmpInst.o: OtherInsts/IcmpInst.h
	g++ -g -c OtherInsts/IcmpInst.cpp -std=c++17

BrInst.o: TerminatorInsts/BrInst.h
	g++ -g -c TerminatorInsts/BrInst.cpp -std=c++17

RetInst.o: TerminatorInsts/RetInst.h
	g++ -g -c TerminatorInsts/RetInst.cpp -std=c++17

LLvmInstruction.o: LLvmInstruction.h
	g++ -g -c LLvmInstruction.cpp -std=c++17

clean:
	rm *.o llvm-out

