// $Id$

#ifndef RAWFRAME_HH
#define RAWFRAME_HH

#include "FrameSource.hh"
#include "build-info.hh"
#include <cassert>

struct SDL_Surface;

namespace openmsx {

/** A video frame as output by the VDP scanline conversion unit,
  * before any postprocessing filters are applied.
  */
class RawFrame : public FrameSource
{
public:
	RawFrame(const SDL_PixelFormat& format, unsigned maxWidth, unsigned height);
	virtual ~RawFrame();

	template<typename Pixel>
	Pixel* getLinePtrDirect(unsigned y) {
		if (PLATFORM_GP2X) {
			if (!isLocked()) lock();
		}
		return reinterpret_cast<Pixel*>(data + y * pitch);
	}

	virtual unsigned getLineBufferSize() const;

	inline void setLineWidth(unsigned line, unsigned width) {
		assert(line < getHeight());
		assert(width <= maxWidth);
		lineWidth[line] = width;
	}

	template <class Pixel>
	inline void setBlank(unsigned line, Pixel color) {
		assert(line < getHeight());
		Pixel* pixels = getLinePtrDirect<Pixel>(line);
		pixels[0] = color;
		lineWidth[line] = 1;
	}

	virtual unsigned getRowLength() const;

	/** Lock this OutputSurface.
	  * Direct pixel access is only allowed on a locked surface.
	  * Locking an already locked surface has no effect.
	  */
	void lock();

	/** Unlock this OutputSurface.
	  * @see lock().
	  */
	void unlock();

	/** Is this OutputSurface currently locked?
	  */
	bool isLocked() const { return locked; }

	SDL_Surface* getSDLSurface() const { return surface; }

protected:
	virtual const void* getLineInfo(unsigned line, unsigned& width) const;
	virtual bool hasContiguousStorage() const;

private:
	SDL_Surface* surface; // only for GP2X
	char* data;
	unsigned* lineWidth;
	unsigned maxWidth;
	unsigned pitch;
	bool locked;
};

} // namespace openmsx

#endif
