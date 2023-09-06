#ifndef STATEMENT_H
#define STATEMENT_H

#include <string>

#include "llvm/IR/Value.h"

class Statement {

    std::string Label;

public:
    Statement(std::string label) : Label(label) {}
    // virtual ~Statement(){}

    // virtual std::string ToString();
    std::string getLabel() const { return Label; }
    void setLabel(const std::string label) { Label = label; }

    virtual llvm::Value* ToLlvmIr() {}
    virtual void dump() const {}

};

#endif