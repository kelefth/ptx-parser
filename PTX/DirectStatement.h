#ifndef DIRECT_STATEMENT_H
#define DIRECT_STATEMENT_H

#include <vector>
#include <memory>

#include "Statement.h"

class DirectStatement : public Statement {

    std::string Directive;

public:
    DirectStatement(
        std::string label,
        std::string directive
    ) : Statement(label), Directive(directive) {}

    std::string getDirective() const;

    void dump() const;
};

#endif