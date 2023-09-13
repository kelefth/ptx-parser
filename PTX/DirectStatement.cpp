#include <iostream>
#include <iomanip>

#include "DirectStatement.h"

// void DirectStatement::dump() {
//     const int colWidth = 20;

//     std::cout << std::left << std::setw(colWidth) << "Label: " + getLabel() << "Directives: ";

//     int index = 0;
//     std::string output = "";
//     for (std::string directive : Directives) {
//         output += directive;
//         index++;
//         if (index < Directives.size()) output += ", ";
//     }

//     std::cout << std::left << std::setw(colWidth) << output;

//     std::cout << std::left << std::setw(colWidth-5) << "Type: " + Type;

//     std::cout << "Arguments: ";
//     index = 0;
//     output = "";
//     for (std::string argument : Arguments) {
//         output += argument;
//         index++;
//         if (index < Arguments.size()) output += ", ";
//     }

//     std::cout << std::left << std::setw(colWidth) << output;
// }

std::string DirectStatement::getDirective() const {
    return Directive;
}

bool DirectStatement::operator==(const Statement stmt) const {
    const DirectStatement* directStmt =
        dynamic_cast<const DirectStatement*>(&stmt);

    if (directStmt == nullptr) return false;

    return getId() == stmt.getId();
}

void DirectStatement::ToLlvmIr() { }

void DirectStatement::dump() const {}