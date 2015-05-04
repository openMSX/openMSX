#ifndef GLTEXTURE_HH
#define GLTEXTURE_HH

#include "BaseImage.hh"
#include "GLUtil.hh"
#include "openmsx.hh"
#include <string>

class SDLSurfacePtr;

namespace openmsx {

class GLImage final : public BaseImage
{
public:
	explicit GLImage(const std::string& filename);
	explicit GLImage(SDLSurfacePtr image);
	GLImage(const std::string& filename, float scaleFactor);
	GLImage(const std::string& filename, gl::ivec2 size);
	GLImage(gl::ivec2 size, unsigned rgba);
	GLImage(gl::ivec2 size, const unsigned* rgba,
	        int borderSize, unsigned borderRGBA);

	void draw(OutputSurface& output, gl::ivec2 pos,
	          byte r, byte g, byte b, byte alpha) override;
	gl::ivec2 getSize() const override { return size; }

private:
	gl::ivec2 size;
	gl::vec2 texCoord;
	gl::Texture texture; // must come after size and texCoord
	int borderSize;
	int bgA[4], borderA;
	byte bgR[4], bgG[4], bgB[4];
	byte borderR, borderG, borderB;
};

} // namespace openmsx

#endif
