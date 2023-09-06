#ifndef MODULE_DIRECT_STATEMENT_H
#define MODULE_DIRECT_STATEMENT_H

#include <vector>

#include "DirectStatement.h"

class ModuleDirectStatement : public DirectStatement {

    std::string Value;

public:

    ModuleDirectStatement(
        std::string label,
        std::string directive,
        std::string value
    );

    ModuleDirectStatement(
        std::string directive,
        std::string value
    );

    std::string ToString() const;

    llvm::Value* ToLlvmIr();
    void dump() const;

};

#endif