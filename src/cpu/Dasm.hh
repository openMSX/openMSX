#ifndef DASM_HH
#define DASM_HH

#include "EmuTime.hh"

#include <cstdint>
#include <functional>
#include <span>
#include <string>

namespace openmsx {

class MSXCPUInterface;

void appendAddrAsHex(std::string& output, uint16_t addr);

/** Disassemble
  * @param interface The CPU interface, used to peek bytes from memory
  * @param pc The position (program counter) where to start disassembling
  * @param buf The bytes that form this opcode (max 4). The buffer is only
  *            filled with the min required bytes (see return value)
  * @param dest String representation of the disassembled opcode
  * @param time TODO
  * @return Length of the disassembled opcode in bytes
  */
unsigned dasm(const MSXCPUInterface& interface, uint16_t pc, std::span<uint8_t, 4> buf,
              std::string& dest, EmuTime::param time,
              std::function<void(std::string&, uint16_t)> appendAddr = &appendAddrAsHex);

unsigned instructionLength(const MSXCPUInterface& interface, uint16_t pc,
                           EmuTime::param time);

} // namespace openmsx

#endif
