#include <iostream>

#include "ModuleDirectStatement.h"

ModuleDirectStatement::ModuleDirectStatement(
    std::string label,
    std::string directive,
    std::string value
) : DirectStatement(label, directive), Value(value) {}


ModuleDirectStatement::ModuleDirectStatement(
    std::string directive,
    std::string value
) : DirectStatement("", directive), Value(value) {}

std::string ModuleDirectStatement::ToString() const {
    return getLabel() + " " + getDirective() + " " + Value;
}

void ModuleDirectStatement::dump() const {
    std::cout << ToString();
}