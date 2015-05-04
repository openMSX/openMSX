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

OSDRectangle::OSDRectangle(OSDGUI& gui, const string& name)
	: OSDImageBasedWidget(gui, name)
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
	Interpreter& interp, string_ref name, const TclObject& value)
{
	if (name == "-w") {
		float w = value.getDouble(interp);
		if (size[0] != w) {
			size[0] = w;
			invalidateRecursive();
		}
	} else if (name == "-h") {
		float h = value.getDouble(interp);
		if (size[1] != h) {
			size[1] = h;
			invalidateRecursive();
		}
	} else if (name == "-relw") {
		float relw = value.getDouble(interp);
		if (relSize[0] != relw) {
			relSize[0] = relw;
			invalidateRecursive();
		}
	} else if (name == "-relh") {
		float relh = value.getDouble(interp);
		if (relSize[1] != relh) {
			relSize[1] = relh;
			invalidateRecursive();
		}
	} else if (name == "-scale") {
		float scale2 = value.getDouble(interp);
		if (scale != scale2) {
			scale = scale2;
			invalidateRecursive();
		}
	} else if (name == "-image") {
		string val = value.getString().str();
		if (imageName != val) {
			if (!val.empty() && !FileOperations::isRegularFile(val)) {
				throw CommandException("Not a valid image file: " + val);
			}
			imageName = val;
			invalidateRecursive();
		}
	} else if (name == "-bordersize") {
		float size = value.getDouble(interp);
		if (borderSize != size) {
			borderSize = size;
			invalidateLocal();
		}
	} else if (name == "-relbordersize") {
		float size = value.getDouble(interp);
		if (relBorderSize != size) {
			relBorderSize = size;
			invalidateLocal();
		}
	} else if (name == "-borderrgba") {
		unsigned newRGBA = value.getInt(interp);
		if (borderRGBA != newRGBA) {
			borderRGBA = newRGBA;
			invalidateLocal();
		}
	} else {
		OSDImageBasedWidget::setProperty(interp, name, value);
	}
}

void OSDRectangle::getProperty(string_ref name, TclObject& result) const
{
	if (name == "-w") {
		result.setDouble(size[0]);
	} else if (name == "-h") {
		result.setDouble(size[1]);
	} else if (name == "-relw") {
		result.setDouble(relSize[0]);
	} else if (name == "-relh") {
		result.setDouble(relSize[1]);
	} else if (name == "-scale") {
		result.setDouble(scale);
	} else if (name == "-image") {
		result.setString(imageName);
	} else if (name == "-bordersize") {
		result.setDouble(borderSize);
	} else if (name == "-relbordersize") {
		result.setDouble(relBorderSize);
	} else if (name == "-borderrgba") {
		result.setInt(borderRGBA);
	} else {
		OSDImageBasedWidget::getProperty(name, result);
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
		return vec2(image->getWidth(), image->getHeight());
	} else {
		return (size * float(getScaleFactor(output)) * scale) +
		       (getParent()->getSize(output) * relSize);
	}
	//std::cout << "rectangle getWH " << getName() << "  " << width << " x " << height << std::endl;
}

byte OSDRectangle::getFadedAlpha() const
{
	return byte(255 * getRecursiveFadeValue());
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
		int bs = int(round(factor * borderSize + iSize[0] * relBorderSize));
		assert(bs >= 0);
		return make_unique<IMAGE>(iSize[0], iSize[1], getRGBA4(), bs, borderRGBA);
	} else {
		string file = systemFileContext().resolve(imageName);
		if (takeImageDimensions()) {
			float factor = getScaleFactor(output) * scale;
			return make_unique<IMAGE>(file, factor);
		} else {
			ivec2 iSize = round(getSize(output));
			return make_unique<IMAGE>(file, iSize[0], iSize[1]);
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
