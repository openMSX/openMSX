#ifndef OUTPUTSURFACE_HH
#define OUTPUTSURFACE_HH

#include "gl_vec.hh"
#include <string>
#include <cassert>
#include <SDL.h>

namespace openmsx {

/** A frame buffer where pixels can be written to.
  * It could be an in-memory buffer or a video buffer visible to the user
  * (see *OffScreenSurface and *VisibleSurface classes).
  */
class OutputSurface
{
public:
	OutputSurface(const OutputSurface&) = delete;
	OutputSurface& operator=(const OutputSurface&) = delete;

	virtual ~OutputSurface() = default;

	int getWidth()  const { return surface->w; }
	int getHeight() const { return surface->h; }
	gl::ivec2 getLogicalSize()  const { return {getWidth(), getHeight()}; }
	gl::ivec2 getPhysicalSize() const { return m_physSize; }

	gl::ivec2 getViewOffset() const { return m_viewOffset; }
	gl::ivec2 getViewSize()   const { return m_viewSize; }
	gl::vec2  getViewScale()  const { return m_viewScale; }
	bool      isViewScaled()  const { return m_viewScale != gl::vec2(1.0f); }

	const SDL_PixelFormat& getSDLFormat() const { return format; }
	SDL_Surface* getSDLSurface()          const { return surface; }
	SDL_Renderer* getSDLRenderer()        const { return renderer; }

	/** Returns the pixel value for the given RGB color.
	  * No effort is made to ensure that the returned pixel value is not the
	  * color key for this output surface.
	  */
	unsigned mapRGB(gl::vec3 rgb)
	{
		return mapRGB255(gl::ivec3(rgb * 255.0f));
	}

	/** Same as mapRGB, but RGB components are in range [0..255].
	 */
	unsigned mapRGB255(gl::ivec3 rgb)
	{
		auto [r, g, b] = rgb;
		return SDL_MapRGB(&format, r, g, b); // alpha is fully opaque
	}

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
	template<typename Pixel> Pixel mapKeyedRGB255(gl::ivec3 rgb)
	{
		Pixel p = mapRGB255(rgb);
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
	template<typename Pixel> Pixel mapKeyedRGB(gl::vec3 rgb)
	{
		return mapKeyedRGB255<Pixel>(gl::ivec3(rgb * 255.0f));
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
	  * you don't have direct access to a pixel buffer.
	  */
	template <typename Pixel>
	Pixel* getLinePtrDirect(unsigned y) {
		assert(isLocked());
		return reinterpret_cast<Pixel*>(data + y * pitch);
	}

	/** Copy frame buffer to display buffer.
	  * The default implementation does nothing.
	  */
	virtual void flushFrameBuffer();

	/** Save the content of this OutputSurface to a PNG file.
	  * @throws MSXException If creating the PNG file fails.
	  */
	virtual void saveScreenshot(const std::string& filename) = 0;

	/** Clear frame buffer (paint it black).
	  * The default implementation does nothing.
	 */
	virtual void clearScreen();

protected:
	OutputSurface() = default;

	void calculateViewPort(gl::ivec2 physSize);
	void setSDLSurface(SDL_Surface* surface_) { surface = surface_; }
	void setSDLRenderer(SDL_Renderer* r) { renderer = r; }
	void setSDLFormat(const SDL_PixelFormat& format);
	void setBufferPtr(char* data, unsigned pitch);

private:
	SDL_Surface* surface = nullptr;
	SDL_Renderer* renderer = nullptr;
	SDL_PixelFormat format;
	char* data;
	unsigned pitch;
	gl::ivec2 m_physSize;
	gl::ivec2 m_viewOffset;
	gl::ivec2 m_viewSize;
	gl::vec2 m_viewScale{1.0f};

	bool locked = false;
};

} // namespace openmsx

#endif
