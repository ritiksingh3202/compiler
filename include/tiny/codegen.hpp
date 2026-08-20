#pragma once

#include "tiny/ir.hpp"
#include <string>

namespace tiny {

std::string emit_asm(const Module& module);

} // namespace tiny
