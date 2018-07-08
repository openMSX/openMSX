#include "BaseImage.hh"
#include "MSXException.hh"

namespace openmsx {

static const int MAX_SIZE = 2048;

void BaseImage::checkSize(gl::ivec2 size)
{
	if (size[0] < -MAX_SIZE || size[0] > MAX_SIZE) {
		throw MSXException("Image width too large: ", size[0],
		                   " (max ", MAX_SIZE, ')');
	}
	if (size[1] < -MAX_SIZE || size[1] > MAX_SIZE) {
		throw MSXException("Image height too large: ", size[1],
		                   " (max ", MAX_SIZE, ')');
	}
}

} // namespace openmsx
