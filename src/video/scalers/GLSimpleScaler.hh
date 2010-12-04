// $Id$

#ifndef GLSIMPLESCALER_HH
#define GLSIMPLESCALER_HH

#include "GLScaler.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class RenderSettings;
class ShaderProgram;

class GLSimpleScaler: public GLScaler, private noncopyable
{
public:
	explicit GLSimpleScaler(RenderSettings& renderSettings);

	virtual void scaleImage(
		ColorTexture& src, ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);

private:
	RenderSettings& renderSettings;
	struct Data {
		std::auto_ptr<ShaderProgram> scalerProgram;
		int texSizeLoc;
		int texStepXLoc;
		int cnstLoc;
	} data[2];
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
