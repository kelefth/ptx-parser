#include "LlvmKernel.h"

LlvmKernel::LlvmKernel(
    std::vector<Type> parameterTypes,
    std::vector<std::unique_ptr<LlvmStatement>> statements
) : ParameterTypes(parameterTypes), Statements(statements) {}
