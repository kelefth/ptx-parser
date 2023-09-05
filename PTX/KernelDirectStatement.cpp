#include <iostream>

#include "KernelDirectStatement.h"

KernelDirectStatement::KernelDirectStatement(
    std::string label,
    std::string name,
    std::vector<std::shared_ptr<ParamDirectStatement>> parameters,
    std::vector<std::shared_ptr<Statement>> bodyStatements
) : DirectStatement(label, ".entry"), Name(name), Parameters(parameters), BodyStatements(bodyStatements) {}

KernelDirectStatement::KernelDirectStatement(
    std::string label,
    std::string name
) : DirectStatement(label, ".entry"), Name(name) {}

KernelDirectStatement::KernelDirectStatement(const KernelDirectStatement& stmt)
: DirectStatement(stmt.getLabel(), stmt.getDirective()) {
    Name = stmt.Name;
    Parameters = stmt.Parameters;
    BodyStatements = stmt.BodyStatements;
}

std::string KernelDirectStatement::getName() {
    return Name;
}

void KernelDirectStatement::AddParameter(std::shared_ptr<ParamDirectStatement> parameter) {
    Parameters.push_back(parameter);
}

void KernelDirectStatement::AddBodyStatement(std::shared_ptr<Statement> statement) {
    BodyStatements.push_back(statement);
}

void KernelDirectStatement::dump() const {
    std::cout << "Name: " << Name << std::endl;
    std::cout << "Parameters: " << std::endl;
    for (auto parameter : Parameters) {
        parameter->dump();
        std::cout << std::endl;
    }

    std::cout << "Body Statements: " << std::endl;
    for (auto statement : BodyStatements) {
        statement->dump();
        std::cout << std::endl;
    }
}