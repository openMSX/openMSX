// $Id$

#ifndef PNG_HH
#define PNG_HH

#include <string>

struct SDL_Surface;
struct SDL_PixelFormat;

namespace openmsx {

/** Utility functions to hide the complexity of saving to a PNG file.
  */
namespace PNG {

	void save(SDL_Surface* image, const std::string& filename);
	void save(unsigned width, unsigned height, const void** rowPointers,
	          const SDL_PixelFormat& format, const std::string& filename);
	void save(unsigned witdh, unsigned height, const void** rowPointers,
	          const std::string& filename);
	void saveGrayscale(unsigned witdh, unsigned height,
	                   const void** rowPointers, const std::string& filename);

} // namespace PNG
} // namespace openmsx

#endif // PNG_HH
