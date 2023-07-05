#ifndef PARSER_H
#define PARSER_H

#include "../lexer/lexer.h"
#include "../InstrStatement.h"

#include <memory>

// extern std::vector<std::unique_ptr<Statement>> statements;
// extern std::vector<InstrStatement> statements;
extern std::vector<std::unique_ptr<Statement>> statements;

bool isInstrToken(int token);

void ParseInstrStatement();


#endif