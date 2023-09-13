#ifndef DIRECT_STATEMENT_H
#define DIRECT_STATEMENT_H

#include <vector>
#include <memory>

#include "llvm/IR/Value.h"

#include "Statement.h"

class DirectStatement : public Statement {

    std::string Directive;

public:
    DirectStatement(
        unsigned int id,
        std::string label,
        std::string directive
    ) : Statement(id, label), Directive(directive) {}

    std::string getDirective() const;

    bool operator==(const Statement stmt) const;

    void ToLlvmIr();
    void dump() const;
};

#endif