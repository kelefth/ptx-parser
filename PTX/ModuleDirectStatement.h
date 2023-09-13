#ifndef MODULE_DIRECT_STATEMENT_H
#define MODULE_DIRECT_STATEMENT_H

#include <vector>

#include "DirectStatement.h"

class ModuleDirectStatement : public DirectStatement {

    std::string Value;

public:

    ModuleDirectStatement(
        unsigned int id,
        std::string label,
        std::string directive,
        std::string value
    );

    ModuleDirectStatement(
        unsigned int id,
        std::string directive,
        std::string value
    );

    bool operator==(const Statement stmt) const;

    std::string ToString() const;

    void ToLlvmIr();
    void dump() const;

};

#endif