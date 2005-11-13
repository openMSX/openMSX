// $Id$

#ifndef SCREENSHOTSAVER_HH
#define SCREENSHOTSAVER_HH

#include "openmsx.hh"
#include <string>

struct SDL_Surface;

namespace openmsx {

/** Utility functions to hide the complexity of saving to a .png file
  */
namespace ScreenShotSaver {

	void save(SDL_Surface* image, const std::string& filename);
	void save(unsigned witdh, unsigned height,
	          byte** row_pointers, const std::string& filename);

} // namespace ScreenShotSaver

} // namespace openmsx

#endif
