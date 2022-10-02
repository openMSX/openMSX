#ifndef DASMTABLES_HH
#define DASMTABLES_HH

#include <array>

namespace openmsx {

extern std::array<const char*, 256> mnemonic_xx_cb;
extern std::array<const char*, 256> mnemonic_cb;
extern std::array<const char*, 256> mnemonic_ed;
extern std::array<const char*, 256> mnemonic_xx;
extern std::array<const char*, 256> mnemonic_main;

} // namespace openmsx

#endif
