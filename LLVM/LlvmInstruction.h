#include <string>

class LlvmInstruction {
    std::string Name;

public:

    LlvmInstruction(std::string name);

    std::string getName();
};

enum Type {
    i8,
    i16,
    i32,
    i64,
    half,
    floatT,
    doubleT,
    i1
};

static const char *enumToString[] = {
    "i8",
    "i16",
    "i32",
    "i64",
    "half",
    "float",
    "double",
    "i1"
};

class Operand {
    std::string Name;
    Type OpType;
    int PtrDepth;

public:

    Operand(std::string name, Type type);
    Operand(std::string name, Type type, int PtrDepth);

    Type getType();
};
