// $Id$

#ifndef DASM_HH
#define DASM_HH

#include "openmsx.hh"
#include <string>

namespace openmsx {

/** Disassemble first opcode in buffer.
  * @return length of given opcode in bytes
  */
int dasm(const byte* buffer, unsigned pc, std::string& dest);

} // namespace openmsx

#endif
