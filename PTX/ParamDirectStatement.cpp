#include <iostream>

#include "ParamDirectStatement.h"

ParamDirectStatement::ParamDirectStatement(
    unsigned int id,
    std::string label,
    std::string name,
    std::string type,
    int alignment,
    int size
) : DirectStatement(id, label, ".param"),
    Name(name),
    Type(type),
    Alignment(alignment),
    Size(size) {}

ParamDirectStatement::ParamDirectStatement(
    unsigned int id,
    std::string name,
    std::string type,
    int alignment,
    int size
) : DirectStatement(id, "", ".param"),
    Name(name), Type(type),
    Alignment(alignment),
    Size(size) {}

std::string ParamDirectStatement::getName() {
    return Name;
}

std::string ParamDirectStatement::getType() {
    return Type;
}

int ParamDirectStatement::getAlignment() {
    return Alignment;
}

int ParamDirectStatement::getSize() {
    return Size;
}

bool ParamDirectStatement::operator==(const Statement stmt) const {
    const ParamDirectStatement* paramStmt =
        dynamic_cast<const ParamDirectStatement*>(&stmt);

    if (paramStmt == nullptr) return false;

    return getId() == stmt.getId();
}

std::string ParamDirectStatement::ToString() {
    return "";
}

void ParamDirectStatement::ToLlvmIr() { }

void ParamDirectStatement::dump() const {
    std::cout << getLabel() << " .param "
              << Name << " " 
              << "Type: " << Type << " "
              << "Align: " << Alignment << " "
              << "Size: " << Size << " ";
}