// $Id$

#ifndef GLSIMPLESCALER_HH
#define GLSIMPLESCALER_HH

#include "GLScaler.hh"
#include <memory>

namespace openmsx {

class RenderSettings;
class ShaderProgram;

class GLSimpleScaler: public GLScaler
{
public:
	GLSimpleScaler(RenderSettings& renderSettings);

	virtual void scaleImage(
		ColourTexture& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth);

private:
	RenderSettings& renderSettings;
	std::auto_ptr<ShaderProgram> scalerProgram;
	int texSizeLoc;
	int alphaLoc;
	int scanALoc;
	int scanBLoc;
	int scanCLoc;
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
