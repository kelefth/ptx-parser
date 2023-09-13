#include "Statement.h"


Statement::Statement(unsigned int id) : Id(id) {}

Statement::Statement(unsigned int id, std::string label)
    : Id(id), Label(label) {}
// virtual ~Statement(){}

unsigned int Statement::getId() const { return Id; }

// virtual std::string ToString();
std::string Statement::getLabel() const { return Label; }
void Statement::setLabel(const std::string label) { Label = label; }


bool Statement::operator==(const Statement stmt) const {
    return getId() == stmt.getId();
};

void Statement::ToLlvmIr() {}
void Statement::dump() const {}