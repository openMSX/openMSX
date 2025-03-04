#ifndef BITMAPCONVERTER_HH
#define BITMAPCONVERTER_HH

#include "DisplayMode.hh"
#include "openmsx.hh"

#include <array>
#include <cstdint>
#include <span>

namespace openmsx {

/** Utility class for converting VRAM contents to host pixels.
  */
class BitmapConverter
{
public:
	using Pixel = uint32_t;
	using DPixel = uint64_t;

	/** Create a new bitmap scanline converter.
	  * @param palette16 Pointer to 2*16-entries array that specifies
	  *   VDP color index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine. Though to allow
	  *   some internal optimizations, this class should be informed about
	  *   changes in this array (not needed for the next two), see
	  *   palette16Changed().
	  *   Used for display modes Graphic4, Graphic5 and Graphic6.
	  *   First 16 entries are for even pixels, next 16 are for odd pixels
	  * @param palette256 Pointer to 256-entries array that specifies
	  *   VDP color index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  *   Used for display mode Graphic7.
	  * @param palette32768 Pointer to 32768-entries array that specifies
	  *   VDP color index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  *   Used when YJK filter is active.
	  */
	BitmapConverter(std::span<const Pixel, 16 * 2> palette16,
	                std::span<const Pixel, 256>    palette256,
	                std::span<const Pixel, 32768>  palette32768);

	/** Convert a line of V9938 VRAM to 256 or 512 host pixels.
	  * Call this method in non-planar display modes (Graphic4 and Graphic5).
	  * @param buf Buffer where host pixels will be written to.
	  *            Depending on the screen mode, the buffer must contain at
	  *            least 256 or 512 pixels, but more is allowed (the extra
	  *            pixels aren't touched).
	  * @param vramPtr Pointer to VRAM contents.
	  */
	void convertLine(std::span<Pixel> buf, std::span<const byte, 128> vramPtr);

	/** Convert a line of V9938 VRAM to 256 or 512 host pixels.
	  * Call this method in planar display modes (Graphic6 and Graphic7).
	  * @param buf Buffer where host pixels will be written to.
	  *            Depending on the screen mode, the buffer must contain at
	  *            least 256 or 512 pixels, but more is allowed (the extra
	  *            pixels aren't touched).
	  * @param vramPtr0 Pointer to VRAM contents, first plane.
	  * @param vramPtr1 Pointer to VRAM contents, second plane.
	  */
	void convertLinePlanar(std::span<Pixel> buf,
	                       std::span<const byte, 128> vramPtr0,
	                       std::span<const byte, 128> vramPtr1);

	/** Select the display mode to use for scanline conversion.
	  * @param mode_ The new display mode.
	  */
	void setDisplayMode(DisplayMode mode_)
	{
		mode = mode_;
	}

	/** Inform this class about changes in the palette16 array.
	  */
	void palette16Changed()
	{
		dPaletteValid = false;
	}

private:
	void calcDPalette();

	void renderGraphic4(std::span<Pixel, 256> buf,
	                    std::span<const byte, 128> vramPtr0);
	void renderGraphic5(std::span<Pixel, 512> buf,
	                    std::span<const byte, 128> vramPtr0) const;
	void renderGraphic6(std::span<Pixel, 512> buf,
	                    std::span<const byte, 128> vramPtr0,
	                    std::span<const byte, 128> vramPtr1);
	void renderGraphic7(std::span<Pixel, 256> buf,
	                    std::span<const byte, 128> vramPtr0,
	                    std::span<const byte, 128> vramPtr1) const;
	void renderYJK(     std::span<Pixel, 256> buf,
	                    std::span<const byte, 128> vramPtr0,
	                    std::span<const byte, 128> vramPtr1) const;
	void renderYAE(     std::span<Pixel, 256> buf,
	                    std::span<const byte, 128> vramPtr0,
	                    std::span<const byte, 128> vramPtr1) const;
	void renderBogus(   std::span<Pixel, 256> buf) const;

private:
	std::span<const Pixel, 16 * 2> palette16;
	std::span<const Pixel, 256>    palette256;
	std::span<const Pixel, 32768>  palette32768;

	std::array<DPixel, 16 * 16> dPalette;
	DisplayMode mode;
	bool dPaletteValid = false;
};

} // namespace openmsx

#endif
