#ifndef DIRECT_STATEMENT_H
#define DIRECT_STATEMENT_H

#include <vector>
#include <memory>

#include "Statement.h"

class DirectStatement : public Statement {

    std::vector<std::string> Directives;
    std::string Type;
    std::vector<std::string> Arguments;
    std::vector<std::unique_ptr<Statement>> Statements;

public:
    DirectStatement(
        std::string label,
        std::vector<std::string> directives,
        std::string type,
        std::vector<std::string> arguments,
        std::vector<std::unique_ptr<Statement>> statements
    ) : Statement(label), Directives(directives), Type(type), Arguments(arguments), Statements(std::move(statements)) {}

    void dump();
};

#endif