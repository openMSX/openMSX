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
		ColourTexture& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth);

private:
	RenderSettings& renderSettings;
	std::auto_ptr<ShaderProgram> scalerProgram;
	int texSizeLoc;
	int texStepXLoc;
	int cnstLoc;
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
