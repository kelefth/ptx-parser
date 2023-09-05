#include <vector>
#include <memory>

#include "LlvmStatement.h"

class LlvmKernel {

    std::vector<Type> ParameterTypes;
    std::vector<std::unique_ptr<LlvmStatement>> Statements;

public:
    LlvmKernel(
        std::vector<Type> parameterTypes,
        std::vector<std::unique_ptr<LlvmStatement>>
    );
};