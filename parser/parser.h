#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <memory>

#include "../lexer/lexer.h"
#include "../Statement.h"


// extern std::vector<std::unique_ptr<Statement>> statements;
// extern std::vector<InstrStatement> statements;
extern std::vector<std::unique_ptr<Statement>> statements;

bool isInstrToken(int token);

void ParseInstrStatement();
void ParseDirectStatement();


#endif