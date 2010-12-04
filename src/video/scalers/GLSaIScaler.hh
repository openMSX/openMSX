// $Id$

#ifndef GLSAISCALER_HH
#define GLSAISCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class ShaderProgram;

class GLSaIScaler : public GLScaler, private noncopyable
{
public:
	GLSaIScaler();

	virtual void scaleImage(
		ColorTexture& src, ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);

private:
	std::auto_ptr<ShaderProgram> scalerProgram[2];
	GLint texSizeLoc[2];
};

} // namespace openmsx

#endif
