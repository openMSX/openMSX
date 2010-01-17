// $Id$

#ifndef GLTVSCALER_HH
#define GLTVSCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class RenderSettings;
class ShaderProgram;

class GLTVScaler: public GLScaler, private noncopyable
{
public:
	explicit GLTVScaler(RenderSettings& renderSettings);
	~GLTVScaler();

	virtual void scaleImage(
		ColorTexture& src, ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);

private:
	RenderSettings& renderSettings;
	std::auto_ptr<ShaderProgram> scalerProgram[2];
	GLint texSizeLoc[2];
	GLint minScanlineLoc[2];
	GLint sizeVarianceLoc[2];
};

} // namespace openmsx

#endif // GLTVSCALER_HH
