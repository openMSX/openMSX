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
	PartialColourTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth
	)
{
	src.drawRect(
		0, srcStartY, srcWidth, srcEndY - srcStartY,
		0, dstStartY, dstWidth, dstEndY - dstStartY
		);
}

} // namespace openmsx
