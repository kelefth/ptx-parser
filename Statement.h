#ifndef STATEMENT_H
#define STATEMENT_H

#include <string>

class Statement {

    std::string Label;

public:
    Statement(std::string label) : Label(label) {}
    // virtual ~Statement(){}

    // virtual std::string ToString();
    std::string getLabel() { return Label; }

    virtual void dump() {}

};

#endif