// $Id$

#ifndef OUTPUTSURFACE_HH
#define OUTPUTSURFACE_HH

#include "OutputRectangle.hh"
#include "noncopyable.hh"
#include <string>
#include <cassert>
#include <SDL.h>

namespace openmsx {

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
	int getX() { return xOffset; }
	int getY() { return yOffset; }

	const SDL_PixelFormat& getSDLFormat() { return format; }
	SDL_Surface* getSDLWorkSurface()    const { return workSurface; }
	SDL_Surface* getSDLDisplaySurface() const { return displaySurface; }

	/** Returns the pixel value for the given RGB color.
	  * No effort is made to ensure that the returned pixel value is not the
	  * color key for this output surface.
	  */
	unsigned mapRGB(double dr, double dg, double db);

	/** Returns the color key for this output surface.
	  */
	template<typename Pixel> inline Pixel getKeyColor() const
	{
		return sizeof(Pixel) == 2
			? 0x0001      // lowest bit of 'some' color component is set
			: 0x00000000; // alpha = 0
	}

	/** Returns a color that is visually very close to the key color.
	  * The returned color can be used as an alternative for pixels that would
	  * otherwise have the key color.
	  */
	template<typename Pixel> inline Pixel getKeyColorClash() const
	{
		assert(sizeof(Pixel) != 4); // shouldn't get clashes in 32bpp
		return 0; // is visually very close, practically
		          // indistinguishable, from the actual KeyColor
	}

	/** Returns the pixel value for the given RGB color.
	  * It is guaranteed that the returned pixel value is different from the
	  * color key for this output surface.
	  */
	template<typename Pixel> Pixel mapKeyedRGB(int r8, int g8, int b8)
	{
		Pixel p = SDL_MapRGB(&format, r8, g8, b8);
		if (sizeof(Pixel) == 2) {
			return (p != getKeyColor<Pixel>())
				? p
				: getKeyColorClash<Pixel>();
		} else {
			assert(p != getKeyColor<Pixel>());
			return p;
		}
	}

	/** Returns the pixel value for the given RGB color.
	  * It is guaranteed that the returned pixel value is different from the
	  * color key for this output surface.
	  */
	template<typename Pixel> Pixel mapKeyedRGB(double dr, double dg, double db)
	{
		int r8 = int(dr * 255.0);
		int g8 = int(dg * 255.0);
		int b8 = int(db * 255.0);
		return mapKeyedRGB<Pixel>(r8, g8, b8);
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

	/** For SDLGL-FB-nn, copy frame buffer to OpenGL display.
	  * The default implementation does nothing.
	  */
	virtual void flushFrameBuffer();

	/** Save the content of this OutputSurface to a PNG file.
	  * @throws MSXException If creating the PNG file fails.
	  */
	virtual void saveScreenshot(const std::string& filename) = 0;

protected:
	OutputSurface();
	void setPosition(int x, int y);
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
	int xOffset, yOffset;

	bool locked;

	friend class SDLGLOutputSurface; // for setBufferPtr()
};

} // namespace openmsx

#endif
