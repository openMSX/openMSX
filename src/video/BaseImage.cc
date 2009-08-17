// $Id$

#include "BaseImage.hh"
#include "MSXException.hh"
#include "StringOp.hh"

namespace openmsx {

static int MAX_SIZE = 2048;

using StringOp::Builder;

void BaseImage::checkSize(int width, int height)
{
	if (width < -MAX_SIZE || width > MAX_SIZE) {
		throw MSXException(
			Builder() << "Image width too large: " << width
			          << " (max " << MAX_SIZE << ")"
			);
	}
	if (height < -MAX_SIZE || height > MAX_SIZE) {
		throw MSXException(
			Builder() << "Image height too large: " << height
			          << " (max " << MAX_SIZE << ")"
			);
	}
}

} // namespace openmsx
