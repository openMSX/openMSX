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
	GLImage(const std::string& filename, int width, int height);
	GLImage(int width, int height, unsigned rgba);
	GLImage(int width, int height, const unsigned* rgba,
	        int borderSize, unsigned borderRGBA);

	void draw(OutputSurface& output, int x, int y,
	          byte r, byte g, byte b, byte alpha) override;
	int getWidth()  const override { return width; }
	int getHeight() const override { return height; }

private:
	gl::Texture texture;
	int width;
	int height;
	GLfloat texCoord[4];
	int borderSize;
	int bgA[4], borderA;
	byte bgR[4], bgG[4], bgB[4];
	byte borderR, borderG, borderB;
};

} // namespace openmsx

#endif
