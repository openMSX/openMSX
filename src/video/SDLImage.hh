#ifndef SDLIMAGE_HH
#define SDLIMAGE_HH

#include "BaseImage.hh"
#include "SDLSurfacePtr.hh"
#include <string>

namespace openmsx {

class SDLImage final : public BaseImage
{
public:
	SDLImage(OutputSurface& output, const std::string& filename);
	SDLImage(OutputSurface& output, SDLSurfacePtr image);
	SDLImage(OutputSurface& output, const std::string& filename, float scaleFactor);
	SDLImage(OutputSurface& output, const std::string& filename, gl::ivec2 size);
	SDLImage(OutputSurface& output, gl::ivec2 size, unsigned rgba);
	SDLImage(OutputSurface& output, gl::ivec2 size, const unsigned* rgba,
	         unsigned borderSize, unsigned borderRGBA);

	void draw(OutputSurface& output, gl::ivec2 pos,
	          uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) override;
	gl::ivec2 getSize() const override { return size; }

private:
	SDLTexturePtr toTexture(OutputSurface& output, SDL_Surface& surface);
	SDLTexturePtr loadImage(OutputSurface& output, const std::string& filename);
	void initSolid(OutputSurface& output, gl::ivec2 size, unsigned rgba,
	               unsigned borderSize, unsigned borderRGBA);
	void initGradient(OutputSurface& output, gl::ivec2 size, const unsigned* rgba,
	                  unsigned borderSize, unsigned borderRGBA);

	gl::ivec2 size; // always positive size
	SDLTexturePtr texture; // can be nullptr (must come after size)
	const bool flipX, flipY; // true when original size was negative
};

} // namespace openmsx

#endif
