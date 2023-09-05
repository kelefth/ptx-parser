#include "LlvmInstruction.h"


class LlvmStatement {

    std::string Lhs;
    LlvmInstruction Rhs;

public:
    LlvmStatement(std::string lhs, LlvmInstruction rhs);
};