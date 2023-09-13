#ifndef INSTR_STATEMENT_H
#define INSTR_STATEMENT_H

#include <string>
#include <vector>
#include <variant>
#include <memory>

#include "llvm/IR/Value.h"

#include "Statement.h"
#include "Operand.h"

class InstrStatement : public Statement {

    std::string Pred;
    std::string Inst;
    std::vector<std::string> Modifiers;
    std::vector<std::string> Types;
    // std::vector<std::variant<std::string, double>> DestOps;
    // std::vector<std::variant<std::string, double>> SourceOps;
    std::vector<std::unique_ptr<Operand>> DestOps;
    std::vector<std::unique_ptr<Operand>> SourceOps;

public:
    InstrStatement(
        unsigned int id,
        std::string label,
        std::string pred,
        std::string inst,
        std::vector<std::string> modifiers,
        std::vector<std::string> types, 
        // std::vector<std::variant<std::string, double>> destOps,
        // std::vector<std::variant<std::string, double>> sourceOps
        std::vector<std::unique_ptr<Operand>> destOps,
        std::vector<std::unique_ptr<Operand>> sourceOps
    )
    : Statement(id, label),
      Pred(pred),
      Inst(inst),
      Modifiers(modifiers),
      Types(types),
      DestOps(std::move(destOps)),
      SourceOps(std::move(sourceOps)) {}

    // std::string ToString();
    std::vector<std::unique_ptr<Operand>>& getSourceOps();

    bool operator==(const Statement stmt) const;

    void ToLlvmIr();
    void dump() const;

};


#endif