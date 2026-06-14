/** \file MSXCharacterSets.hh
 * MSX Character Set Data for printers and plotters.
 * Extracted from Printer.hh for shared use.
 */

#ifndef MSX_CHARACTER_SETS_HH
#define MSX_CHARACTER_SETS_HH

#include <cstdint>
#include <span>

namespace openmsx {

// International MSX font (from NMS8250 BIOS ROM)
[[nodiscard]] std::span<const uint8_t, 8 * 256> getMSXFontRaw();

// DIN MSX font (International with modified zero)
[[nodiscard]] std::span<const uint8_t, 8 * 256> getMSXDINFontRaw();

// International MSX font, rotated 90 degrees and padded to 9 dots high.
[[nodiscard]] std::span<const uint8_t, 9 * 256> getMSXFontRotated();

// Japanese MSX font (from Sony F1XV BIOS ROM)
[[nodiscard]] std::span<const uint8_t, 8 * 256> getMSXJPFontRaw();

} // namespace openmsx

#endif
