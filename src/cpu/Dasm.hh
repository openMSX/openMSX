// $Id$

#ifndef DASM_HH
#define DASM_HH

#include "openmsx.hh"
#include <string>

class MSXCPUInterface;

namespace openmsx {

/** Disassemble
  * @param interf The CPU interface, used to peek bytes from memory
  * @param pc The position (program counter) where to start disassembling
  * @param buf The bytes that form this opcode (max 4). The buffer is only
  *            filled with the min required bytes (see return value)
  * @param dest String representation of the disassembled opcode
  * @return Length of the disassembled opcode in bytes
  */
int dasm(const MSXCPUInterface& interf, word pc, byte buf[4], std::string& dest);

} // namespace openmsx

#endif
