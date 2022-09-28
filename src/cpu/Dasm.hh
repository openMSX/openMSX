#ifndef DASM_HH
#define DASM_HH

#include "EmuTime.hh"
#include "openmsx.hh"
#include <span>
#include <string>

namespace openmsx {

class MSXCPUInterface;

/** Disassemble
  * @param interf The CPU interface, used to peek bytes from memory
  * @param pc The position (program counter) where to start disassembling
  * @param buf The bytes that form this opcode (max 4). The buffer is only
  *            filled with the min required bytes (see return value)
  * @param dest String representation of the disassembled opcode
  * @param time TODO
  * @return Length of the disassembled opcode in bytes
  */
unsigned dasm(const MSXCPUInterface& interf, word pc, std::span<byte, 4> buf,
              std::string& dest, EmuTime::param time);

} // namespace openmsx

#endif
