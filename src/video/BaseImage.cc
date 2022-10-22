#include "BaseImage.hh"
#include "MSXException.hh"

namespace openmsx {

static constexpr int MAX_SIZE = 2048;

void BaseImage::checkSize(gl::ivec2 size)
{
	auto [w, h] = size;
	if (w < -MAX_SIZE || w > MAX_SIZE) {
		throw MSXException("Image width too large: ", w,
		                   " (max ", MAX_SIZE, ')');
	}
	if (h < -MAX_SIZE || h > MAX_SIZE) {
		throw MSXException("Image height too large: ", h,
		                   " (max ", MAX_SIZE, ')');
	}
}

} // namespace openmsx
