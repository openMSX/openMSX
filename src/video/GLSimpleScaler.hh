// $Id$

#ifndef GLSIMPLESCALER_HH
#define GLSIMPLESCALER_HH

#include "GLScaler.hh"

namespace openmsx {

class RenderSettings;

class GLSimpleScaler: public GLScaler
{
public:
	GLSimpleScaler(RenderSettings& renderSettings);

	virtual void scaleImage(
		PartialColourTexture& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth
		);

private:
	RenderSettings& renderSettings;
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
