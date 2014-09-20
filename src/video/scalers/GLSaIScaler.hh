#ifndef GLSAISCALER_HH
#define GLSAISCALER_HH

#include "GLScaler.hh"
#include "noncopyable.hh"

namespace openmsx {

class GLSaIScaler final : public GLScaler, private noncopyable
{
public:
	GLSaIScaler();

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight) override;
};

} // namespace openmsx

#endif
