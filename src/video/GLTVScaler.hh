// $Id$

#ifndef GLTVSCALER_HH
#define GLTVSCALER_HH

#include "GLScaler.hh"
#include <memory>

namespace openmsx {

class ShaderProgram;

class GLTVScaler: public GLScaler
{
public:
	GLTVScaler();
	~GLTVScaler();

	virtual void scaleImage(
		PartialColourTexture& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth
		);

private:
	std::auto_ptr<ShaderProgram> scalerProgram;
};

} // namespace openmsx

#endif // GLTVSCALER_HH
