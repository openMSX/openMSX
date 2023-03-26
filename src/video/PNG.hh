#ifndef PNG_HH
#define PNG_HH

#include "SDLSurfacePtr.hh"
#include <span>
#include <string>

/** Utility functions to hide the complexity of saving to a PNG file.
  */
namespace openmsx::PNG {
	/** Load the given PNG file in a SDL_Surface.
	 * This SDL_Surface is either 24bpp or 32bpp, depending on whether the
	 * PNG file had an alpha layer. But it's possible to force a 32bpp
	 * surface. The surface will use RGB(A) or BGR(A) format depending on
	 * the current display format.
	 */
	[[nodiscard]] SDLSurfacePtr load(const std::string& filename, bool want32bpp);

	void saveRGBA(size_t width, std::span<const void*> rowPointers,
	              const std::string& filename);
	void saveGrayscale(size_t width, std::span<const void*> rowPointers,
	                   const std::string& filename);

} // namespace openmsx::PNG

#endif // PNG_HH
