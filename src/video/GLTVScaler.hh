// $Id$

#ifndef GLTVSCALER_HH
#define GLTVSCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class ShaderProgram;

class GLTVScaler: public GLScaler, private noncopyable
{
public:
	GLTVScaler();
	~GLTVScaler();

	virtual void scaleImage(
		ColorTexture& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth);

private:
	std::auto_ptr<ShaderProgram> scalerProgram;
	GLint texSizeLoc;
};

} // namespace openmsx

#endif // GLTVSCALER_HH
