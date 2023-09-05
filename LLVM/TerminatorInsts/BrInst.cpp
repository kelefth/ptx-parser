#include "BrInst.h"

BrInst::BrInst(std::string labelTrue)
    : LlvmInstruction("br"), LabelTrue(labelTrue) {}

BrInst::BrInst(
    std::string cond,
    std::string labelTrue,
    std::string labelFalse
)
: LlvmInstruction("br"),
LabelTrue(labelTrue),
LabelFalse(labelFalse) {}
