// $Id$

#ifndef __DASM_HH__
#define __DASM_HH__

namespace openmsx {

/** Disassemble first opcode in buffer.
  * @return length of given opcode in bytes
  */
int Dasm(unsigned char *buffer, char *dest, unsigned int PC);

} // namespace openmsx

#endif // __DASM_HH__

