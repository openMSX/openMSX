#ifndef SDLIMAGE_HH
#define SDLIMAGE_HH

#include "BaseImage.hh"
#include "SDLSurfacePtr.hh"
#include <span>
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
	SDLImage(OutputSurface& output, gl::ivec2 size, std::span<const unsigned, 4> rgba,
	         unsigned borderSize, unsigned borderRGBA);

	void draw(OutputSurface& output, gl::ivec2 pos,
	          uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) override;

private:
	[[nodiscard]] SDLTexturePtr toTexture(OutputSurface& output, SDL_Surface& surface);
	[[nodiscard]] SDLTexturePtr loadImage(OutputSurface& output, const std::string& filename);
	void initSolid(OutputSurface& output, gl::ivec2 size, unsigned rgba,
	               unsigned borderSize, unsigned borderRGBA);
	void initGradient(OutputSurface& output, gl::ivec2 size, std::span<const unsigned, 4> rgba,
	                  unsigned borderSize, unsigned borderRGBA);

private:
	SDLTexturePtr texture; // can be nullptr
	const bool flipX, flipY; // 'size' is always positive, there are true
	                         // when the original size was negative
};

} // namespace openmsx

#endif
