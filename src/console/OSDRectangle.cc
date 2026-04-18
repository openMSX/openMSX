#include "OSDRectangle.hh"

#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "GLImage.hh"
#include "TclObject.hh"

#include "stl.hh"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>

using namespace gl;

namespace openmsx {

OSDRectangle::OSDRectangle(Display& display_, const TclObject& name_)
	: OSDImageBasedWidget(display_, name_)
{
}

void OSDRectangle::setProperty(
	Interpreter& interp, std::string_view propName, const TclObject& value)
{
	if (propName == "-w") {
		float w = value.getFloat(interp);
		if (size.x != w) {
			size.x = w;
			invalidateRecursive();
		}
	} else if (propName == "-h") {
		float h = value.getFloat(interp);
		if (size.y != h) {
			size.y = h;
			invalidateRecursive();
		}
	} else if (propName == "-relw") {
		float relw = value.getFloat(interp);
		if (relSize.x != relw) {
			relSize.x = relw;
			invalidateRecursive();
		}
	} else if (propName == "-relh") {
		float relh = value.getFloat(interp);
		if (relSize.y != relh) {
			relSize.y = relh;
			invalidateRecursive();
		}
	} else if (propName == "-scale") {
		float scale2 = value.getFloat(interp);
		if (scale != scale2) {
			scale = scale2;
			invalidateRecursive();
		}
	} else if (propName == "-image") {
		std::string_view val = value.getString();
		if (imageName != val) {
			if (!val.empty()) {
				if (auto file = systemFileContext().resolve(val);
				    !FileOperations::isRegularFile(file)) {
					throw CommandException("Not a valid image file: ", val);
				}
			}
			imageName = val;
			invalidateRecursive();
		}
	} else if (propName == "-bordersize") {
		float newSize = value.getFloat(interp);
		if (borderSize != newSize) {
			borderSize = newSize;
			invalidateLocal();
		}
	} else if (propName == "-relbordersize") {
		float newSize = value.getFloat(interp);
		if (relBorderSize != newSize) {
			relBorderSize = newSize;
			invalidateLocal();
		}
	} else if (propName == "-borderrgba") {
		uint32_t newRGBA = value.getInt(interp);
		if (borderRGBA != newRGBA) {
			borderRGBA = newRGBA;
			invalidateLocal();
		}
	} else {
		OSDImageBasedWidget::setProperty(interp, propName, value);
	}
}

void OSDRectangle::getProperty(std::string_view propName, TclObject& result) const
{
	if (propName == "-w") {
		result = size.x;
	} else if (propName == "-h") {
		result = size.y;
	} else if (propName == "-relw") {
		result = relSize.x;
	} else if (propName == "-relh") {
		result = relSize.y;
	} else if (propName == "-scale") {
		result = scale;
	} else if (propName == "-image") {
		result = imageName;
	} else if (propName == "-bordersize") {
		result = borderSize;
	} else if (propName == "-relbordersize") {
		result = relBorderSize;
	} else if (propName == "-borderrgba") {
		result = borderRGBA;
	} else {
		OSDImageBasedWidget::getProperty(propName, result);
	}
}

std::string_view OSDRectangle::getType() const
{
	return "rectangle";
}

bool OSDRectangle::takeImageDimensions() const
{
	return (size == vec2()) && (relSize == vec2());
}

vec2 OSDRectangle::getSize(gl::ivec2 viewSize) const
{
	if (!imageName.empty() && image && takeImageDimensions()) {
		return vec2(image->getSize());
	} else {
		return (size * getScaleFactor(viewSize) * scale) +
		       (getParent()->getSize(viewSize) * relSize);
	}
}

uint8_t OSDRectangle::getFadedAlpha() const
{
	return uint8_t(255 * getRecursiveFadeValue());
}

std::unique_ptr<GLImage> OSDRectangle::create(gl::ivec2 viewSize)
{
	if (imageName.empty()) {
		if (hasConstantAlpha() && ((getRGBA(0) & 0xff) == 0) &&
		    (((borderRGBA & 0xFF) == 0) || (borderSize == 0.0f))) {
			// optimization: Sometimes it's useful to have a
			//   rectangle that will never be drawn, it only exists
			//   as a parent for sub-widgets. For those cases
			//   creating an IMAGE only wastes memory. So postpone
			//   creating it till alpha changes.
			return nullptr;
		}
		vec2 sz = getSize(viewSize);
		auto factor = getScaleFactor(viewSize) * scale;
		auto bs = gl::ivec2(factor * borderSize + sz * relBorderSize);
		assert(bs.x >= 0 && bs.y >= 0);
		return std::make_unique<GLImage>(round(sz), getRGBA4(), bs, borderRGBA);
	} else {
		auto file = systemFileContext().resolve(imageName);
		if (takeImageDimensions()) {
			auto factor = getScaleFactor(viewSize) * scale;
			return std::make_unique<GLImage>(file, GLImage::Scale(factor));
		} else {
			ivec2 iSize = round(getSize(viewSize));
			return std::make_unique<GLImage>(file, iSize);
		}
	}
}

} // namespace openmsx
