// $Id$

#include "GLSimpleScaler.hh"
#include "GLUtil.hh"

namespace openmsx {

class RenderSettings;

GLSimpleScaler::GLSimpleScaler(RenderSettings& renderSettings_)
	: renderSettings(renderSettings_)
{
}

void GLSimpleScaler::scaleImage(
	ColourTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned /*srcWidth*/,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	GLfloat height = src.getHeight();
	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
}

} // namespace openmsx
