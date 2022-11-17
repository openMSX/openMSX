#ifndef DASMTABLES_HH
#define DASMTABLES_HH

#include <array>

namespace openmsx {

extern const std::array<const char*, 256> mnemonic_xx_cb;
extern const std::array<const char*, 256> mnemonic_cb;
extern const std::array<const char*, 256> mnemonic_ed;
extern const std::array<const char*, 256> mnemonic_xx;
extern const std::array<const char*, 256> mnemonic_main;

} // namespace openmsx

#endif
