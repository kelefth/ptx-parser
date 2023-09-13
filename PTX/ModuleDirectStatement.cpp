#include <iostream>

#include "ModuleDirectStatement.h"

ModuleDirectStatement::ModuleDirectStatement(
    unsigned int id,
    std::string label,
    std::string directive,
    std::string value
) : DirectStatement(id, label, directive), Value(value) {}


ModuleDirectStatement::ModuleDirectStatement(
    unsigned int id,
    std::string directive,
    std::string value
) : DirectStatement(id, "", directive), Value(value) {}

bool ModuleDirectStatement::operator==(const Statement stmt)
const {
    const ModuleDirectStatement* moduleStmt =
        dynamic_cast<const ModuleDirectStatement*>(&stmt);

    if (moduleStmt == nullptr) return false;

    return getId() == stmt.getId();
}

std::string ModuleDirectStatement::ToString() const {
    return getLabel() + " " + getDirective() + " " + Value;
}

void ModuleDirectStatement::ToLlvmIr() { }

void ModuleDirectStatement::dump() const {
    std::cout << ToString();
}