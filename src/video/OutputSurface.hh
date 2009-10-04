// $Id$

#ifndef OUTPUTSURFACE_HH
#define OUTPUTSURFACE_HH

#include "noncopyable.hh"
#include <string>
#include <cassert>
#include <SDL.h>

namespace openmsx {

class OutputRectangle
{
public:
	virtual unsigned getOutputWidth()  const = 0;
	virtual unsigned getOutputHeight() const = 0;
protected:
	virtual ~OutputRectangle() {}
};

/** A frame buffer where pixels can be written to.
  * It could be an in-memory buffer or a video buffer visible to the user
  * (see VisibleSurface subclass).
  */
class OutputSurface : public OutputRectangle, private noncopyable
{
public:
	virtual ~OutputSurface();

	unsigned getWidth() const  { return displaySurface->w; }
	unsigned getHeight() const { return displaySurface->h; }
	const SDL_PixelFormat& getSDLFormat() { return format; }
	SDL_Surface* getSDLWorkSurface()    const { return workSurface; }
	SDL_Surface* getSDLDisplaySurface() const { return displaySurface; }
	unsigned mapRGB(double dr, double dg, double db);
	unsigned mapRGB(int r, int g, int b) {
		return SDL_MapRGB(&format, r, g, b);
	}

	template<typename Pixel> inline Pixel getKeyColor() const
	{
		return sizeof(Pixel) == 2
			? 0x0001      // lowest bit of 'some' color component is set
			: 0x00000000; // alpha = 0
	}
	template<typename Pixel> inline Pixel getKeyColorClash() const
	{
		assert(sizeof(Pixel) != 4); // shouldn't get clashes in 32bpp
		return 0; // is visually very close, practically
		          // indistinguishable, from the actual KeyColor
	}

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

	/** Returns a pointer to the requested line in the pixel buffer.
	  * Not all implementations support this operation, e.g. in SDLGL
	  * (non FB version) you don't have direct access to a pixel buffer.
	  */
	template <typename Pixel>
	Pixel* getLinePtrDirect(unsigned y) {
		assert(isLocked());
		return reinterpret_cast<Pixel*>(data + y * pitch);
	}

	/** For SDLGL-FB-nn, copy frame buffer to openGL display.
	  * The default implementation does nothing.
	  */
	virtual void flushFrameBuffer();

	/** Save the content of this OutputSurface to a png file.
	  */
	virtual void saveScreenshot(const std::string& filename) = 0;

protected:
	OutputSurface();
	void setSDLDisplaySurface(SDL_Surface* surface);
	void setSDLWorkSurface   (SDL_Surface* surface);
	void setSDLFormat(const SDL_PixelFormat& format);
	void setBufferPtr(char* data, unsigned pitch);

private:
	virtual unsigned getOutputWidth()  const { return getWidth(); }
	virtual unsigned getOutputHeight() const { return getHeight(); }

	SDL_Surface* displaySurface;
	SDL_Surface* workSurface;
	SDL_PixelFormat format;
	char* data;
	unsigned pitch;
	unsigned keyColor;

	bool locked;

	friend class SDLGLOutputSurface; // for setBufferPtr()
};

} // namespace openmsx

#endif
