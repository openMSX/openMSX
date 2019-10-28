#include "SDLOutputSurface.hh"

namespace openmsx {

void SDLOutputSurface::lock()
{
	if (isLocked()) return;
	locked = true;
	if (surface && SDL_MUSTLOCK(surface)) {
		// Note: we ignore the return value from SDL_LockSurface()
		SDL_LockSurface(surface);
	}
}

void SDLOutputSurface::unlock()
{
	if (!isLocked()) return;
	locked = false;
	if (surface && SDL_MUSTLOCK(surface)) {
		SDL_UnlockSurface(surface);
	}
}

void SDLOutputSurface::setBufferPtr(char* data_, unsigned pitch_)
{
	data = data_;
	pitch = pitch_;
}

} // namespace openmsx
