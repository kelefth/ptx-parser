all:
	(cd PTX; make all)
	(cd LLVM; make all)

clean:
	(cd PTX; make clean)
	(cd LLVM; make clean)