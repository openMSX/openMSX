#ifndef SDLIMAGE_HH
#define SDLIMAGE_HH

#include "BaseImage.hh"
#include "SDLSurfacePtr.hh"
#include <string>

namespace openmsx {

class SDLImage final : public BaseImage
{
public:
	explicit SDLImage(const std::string& filename);
	explicit SDLImage(SDLSurfacePtr image);
	SDLImage(const std::string& filename, float scaleFactor);
	SDLImage(const std::string& filename, gl::ivec2 size);
	SDLImage(gl::ivec2 size, unsigned rgba);
	SDLImage(gl::ivec2 size, const unsigned* rgba,
	         unsigned borderSize, unsigned borderRGBA);

	void draw(OutputSurface& output, gl::ivec2 pos,
	          byte r, byte g, byte b, byte alpha) override;
	gl::ivec2 getSize() const override;

private:
	void initSolid(gl::ivec2 size, unsigned rgba,
	               unsigned borderSize, unsigned borderRGBA);
	void initGradient(gl::ivec2 size, const unsigned* rgba,
	                  unsigned borderSize, unsigned borderRGBA);
	void allocateWorkImage();

	SDLSurfacePtr image;
	SDLSurfacePtr workImage;
	int a; // whole surface alpha value, -1 if per-pixel alpha
	const bool flipX, flipY;
};

} // namespace openmsx

#endif
