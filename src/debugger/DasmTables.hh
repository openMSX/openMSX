#ifndef DASMTABLES_HH
#define DASMTABLES_HH

#include <array>
#include <cstdint>

namespace openmsx {

extern const std::array<const char*, 256> mnemonic_xx_cb;
extern const std::array<const char*, 256> mnemonic_cb;
extern const std::array<const char*, 256> mnemonic_ed;
extern const std::array<const char*, 256> mnemonic_xx;
extern const std::array<const char*, 256> mnemonic_main;

extern const std::array<uint8_t, 3uz * 256> instr_len_tab;

} // namespace openmsx

#endif
