#ifndef STATEMENT_H
#define STATEMENT_H

#include <string>

#include "llvm/IR/Value.h"

#define STACK_POINTER "%SP"

class Statement {

    unsigned int Id;
    std::string Label;

public:
    Statement(unsigned int id);

    Statement(unsigned int id, std::string label);
    // virtual ~Statement(){}

    unsigned int getId() const;

    // virtual std::string ToString();
    std::string getLabel() const;
    void setLabel(const std::string label);

    virtual bool operator==(const Statement stmt) const;

    virtual void ToLlvmIr();
    virtual void dump() const;

};

#endif