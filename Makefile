# LLVM_BUILD_PATH := $$HOME/llvm-project/llvm/build
LLVM_BUILD_PATH := $$HOME/llvm-project/build
LLVM_BIN_PATH 	:= $(LLVM_BUILD_PATH)/bin

CXX := g++
CXXFLAGS := -g -std=c++17

LLVM_FLAGS := `$(LLVM_BIN_PATH)/llvm-config --cxxflags --ldflags --libs analysis --system-libs --libs core` -frtti

SRC_PTX_DIR := PTX
SRC_PTX_LEXER_DIR := $(SRC_PTX_DIR)/lexer
SRC_PTX_PARSER_DIR := $(SRC_PTX_DIR)/parser
SRC_PTXTOIR_DIR := PtxToLlvmIr
BUILDDIR := build

OBJ_FILES := $(BUILDDIR)/lexer.o \
			 $(BUILDDIR)/parser.o \
			 $(BUILDDIR)/Statement.o \
			 $(BUILDDIR)/InstrStatement.o \
			 $(BUILDDIR)/DirectStatement.o \
			 $(BUILDDIR)/Operand.o \
			 $(BUILDDIR)/AddressExpr.o \
			 $(BUILDDIR)/ModuleDirectStatement.o \
			 $(BUILDDIR)/LinkingDirectStatement.o \
			 $(BUILDDIR)/ParamDirectStatement.o \
			 $(BUILDDIR)/KernelDirectStatement.o \
			 $(BUILDDIR)/PtxToLlvmIrConverter.o


.PHONY: make_builddir
make_builddir:
	@test -d $(BUILDDIR) || mkdir $(BUILDDIR)

all: make_builddir $(BUILDDIR)/ptx-parser

$(BUILDDIR)/ptx-parser: $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $^ $(LLVM_FLAGS) -o $@

$(BUILDDIR)/PtxToLlvmIrConverter.o: $(SRC_PTXTOIR_DIR)/PtxToLlvmIrConverter.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTXTOIR_DIR)/PtxToLlvmIrConverter.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/lexer.o: $(SRC_PTX_LEXER_DIR)/lexer.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_LEXER_DIR)/lexer.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/parser.o: $(SRC_PTX_PARSER_DIR)/parser.h $(SRC_PTX_LEXER_DIR)/lexer.h $(SRC_PTX_DIR)/InstrStatement.h $(SRC_PTX_DIR)/DirectStatement.h $(SRC_PTX_DIR)/Operand.h $(SRC_PTX_DIR)/AddressExpr.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_PARSER_DIR)/parser.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/Statement.o: $(SRC_PTX_DIR)/Statement.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_DIR)/Statement.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/InstrStatement.o: $(SRC_PTX_DIR)/InstrStatement.h $(SRC_PTX_DIR)/Statement.h $(SRC_PTX_DIR)/Operand.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_DIR)/InstrStatement.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/DirectStatement.o: $(SRC_PTX_DIR)/DirectStatement.h $(SRC_PTX_DIR)/Statement.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_DIR)/DirectStatement.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/ModuleDirectStatement.o: $(SRC_PTX_DIR)/DirectStatement.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_DIR)/ModuleDirectStatement.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/LinkingDirectStatement.o: $(SRC_PTX_DIR)/DirectStatement.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_DIR)/LinkingDirectStatement.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/ParamDirectStatement.o: $(SRC_PTX_DIR)/DirectStatement.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_DIR)/ParamDirectStatement.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/KernelDirectStatement.o: $(SRC_PTX_DIR)/Statement.h $(SRC_PTX_DIR)/DirectStatement.h $(SRC_PTX_DIR)/ParamDirectStatement.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_DIR)/KernelDirectStatement.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/Operand.o: $(SRC_PTX_DIR)/Operand.h $(SRC_PTX_DIR)/AddressExpr.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_DIR)/Operand.cpp $(LLVM_FLAGS) -o $@

$(BUILDDIR)/AddressExpr.o: $(SRC_PTX_DIR)/AddressExpr.h $(SRC_PTX_DIR)/Operand.h
	$(CXX) $(CXXFLAGS) -c $(SRC_PTX_DIR)/AddressExpr.cpp $(LLVM_FLAGS) -o $@

clean:
	rm -rf $(BUILDDIR)