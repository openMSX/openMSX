#include "OSDRectangle.hh"
#include "SDLImage.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "components.hh"
#include "memory.hh"
#include <cassert>
#include <cmath>
#if COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;
using std::vector;
using namespace gl;

namespace openmsx {

OSDRectangle::OSDRectangle(Display& display_, const TclObject& name_)
	: OSDImageBasedWidget(display_, name_)
	, scale(1.0), borderSize(0.0), relBorderSize(0.0)
	, borderRGBA(0x000000ff)
{
}

vector<string_ref> OSDRectangle::getProperties() const
{
	auto result = OSDImageBasedWidget::getProperties();
	static const char* const vals[] = {
		"-w", "-h", "-relw", "-relh", "-scale", "-image",
		"-bordersize", "-relbordersize", "-borderrgba",
	};
	result.insert(end(result), std::begin(vals), std::end(vals));
	return result;
}

void OSDRectangle::setProperty(
	Interpreter& interp, string_ref propName, const TclObject& value)
{
	if (propName == "-w") {
		float w = value.getDouble(interp);
		if (size[0] != w) {
			size[0] = w;
			invalidateRecursive();
		}
	} else if (propName == "-h") {
		float h = value.getDouble(interp);
		if (size[1] != h) {
			size[1] = h;
			invalidateRecursive();
		}
	} else if (propName == "-relw") {
		float relw = value.getDouble(interp);
		if (relSize[0] != relw) {
			relSize[0] = relw;
			invalidateRecursive();
		}
	} else if (propName == "-relh") {
		float relh = value.getDouble(interp);
		if (relSize[1] != relh) {
			relSize[1] = relh;
			invalidateRecursive();
		}
	} else if (propName == "-scale") {
		float scale2 = value.getDouble(interp);
		if (scale != scale2) {
			scale = scale2;
			invalidateRecursive();
		}
	} else if (propName == "-image") {
		string val = value.getString().str();
		if (imageName != val) {
			if (!val.empty() && !FileOperations::isRegularFile(val)) {
				throw CommandException("Not a valid image file: " + val);
			}
			imageName = val;
			invalidateRecursive();
		}
	} else if (propName == "-bordersize") {
		float newSize = value.getDouble(interp);
		if (borderSize != newSize) {
			borderSize = newSize;
			invalidateLocal();
		}
	} else if (propName == "-relbordersize") {
		float newSize = value.getDouble(interp);
		if (relBorderSize != newSize) {
			relBorderSize = newSize;
			invalidateLocal();
		}
	} else if (propName == "-borderrgba") {
		unsigned newRGBA = value.getInt(interp);
		if (borderRGBA != newRGBA) {
			borderRGBA = newRGBA;
			invalidateLocal();
		}
	} else {
		OSDImageBasedWidget::setProperty(interp, propName, value);
	}
}

void OSDRectangle::getProperty(string_ref propName, TclObject& result) const
{
	if (propName == "-w") {
		result.setDouble(size[0]);
	} else if (propName == "-h") {
		result.setDouble(size[1]);
	} else if (propName == "-relw") {
		result.setDouble(relSize[0]);
	} else if (propName == "-relh") {
		result.setDouble(relSize[1]);
	} else if (propName == "-scale") {
		result.setDouble(scale);
	} else if (propName == "-image") {
		result.setString(imageName);
	} else if (propName == "-bordersize") {
		result.setDouble(borderSize);
	} else if (propName == "-relbordersize") {
		result.setDouble(relBorderSize);
	} else if (propName == "-borderrgba") {
		result.setInt(borderRGBA);
	} else {
		OSDImageBasedWidget::getProperty(propName, result);
	}
}

string_ref OSDRectangle::getType() const
{
	return "rectangle";
}

bool OSDRectangle::takeImageDimensions() const
{
	return (size == vec2()) && (relSize == vec2());
}

vec2 OSDRectangle::getSize(const OutputRectangle& output) const
{
	if (!imageName.empty() && image && takeImageDimensions()) {
		return vec2(image->getSize());
	} else {
		return (size * float(getScaleFactor(output)) * scale) +
		       (getParent()->getSize(output) * relSize);
	}
	//std::cout << "rectangle getWH " << getName() << "  " << width << " x " << height << std::endl;
}

uint8_t OSDRectangle::getFadedAlpha() const
{
	return uint8_t(255 * getRecursiveFadeValue());
}

template <typename IMAGE> std::unique_ptr<BaseImage> OSDRectangle::create(
	OutputRectangle& output)
{
	if (imageName.empty()) {
		bool constAlpha = hasConstantAlpha();
		if (constAlpha && ((getRGBA(0) & 0xff) == 0) &&
		    (((borderRGBA & 0xFF) == 0) || (borderSize == 0.0f))) {
			// optimization: Sometimes it's useful to have a
			//   rectangle that will never be drawn, it only exists
			//   as a parent for sub-widgets. For those cases
			//   creating an IMAGE only wastes memory. So postpone
			//   creating it till alpha changes.
			return nullptr;
		}
		ivec2 iSize = round(getSize(output));
		float factor = getScaleFactor(output) * scale;
		int bs = lrintf(factor * borderSize + iSize[0] * relBorderSize);
		assert(bs >= 0);
		return make_unique<IMAGE>(iSize, getRGBA4(), bs, borderRGBA);
	} else {
		string file = systemFileContext().resolve(imageName);
		if (takeImageDimensions()) {
			float factor = getScaleFactor(output) * scale;
			return make_unique<IMAGE>(file, factor);
		} else {
			ivec2 iSize = round(getSize(output));
			return make_unique<IMAGE>(file, iSize);
		}
	}
}

std::unique_ptr<BaseImage> OSDRectangle::createSDL(OutputRectangle& output)
{
	return create<SDLImage>(output);
}

std::unique_ptr<BaseImage> OSDRectangle::createGL(OutputRectangle& output)
{
#if COMPONENT_GL
	return create<GLImage>(output);
#else
	(void)&output;
	return nullptr;
#endif
}

} // namespace openmsx
