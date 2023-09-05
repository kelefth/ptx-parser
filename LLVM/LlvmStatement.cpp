#include "LlvmStatement.h"

LlvmStatement::LlvmStatement(std::string lhs, LlvmInstruction rhs)
    : Lhs(lhs), Rhs(rhs) {}