// $Id$

#include "OSDRectangle.hh"
#include "SDLImage.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "components.hh"
#include "Math.hh"
#include "memory.hh"
#include <cassert>
#if COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;
using std::set;

namespace openmsx {

OSDRectangle::OSDRectangle(const OSDGUI& gui, const string& name)
	: OSDImageBasedWidget(gui, name)
	, w(0.0), h(0.0), relw(0.0), relh(0.0), scale(1.0)
	, borderSize(0.0), relBorderSize(0.0), borderRGBA(0x000000ff)
{
}

set<string> OSDRectangle::getProperties() const
{
	auto result = OSDImageBasedWidget::getProperties();
	static const char* const vals[] = {
		"-w", "-h", "-relw", "-relh", "-scale", "-image",
		"-bordersize", "-relbordersize", "-borderrgba",
	};
	result.insert(std::begin(vals), std::end(vals));
	return result;
}

void OSDRectangle::setProperty(string_ref name, const TclObject& value)
{
	if (name == "-w") {
		double w2 = value.getDouble();
		if (w != w2) {
			w = w2;
			invalidateRecursive();
		}
	} else if (name == "-h") {
		double h2 = value.getDouble();
		if (h != h2) {
			h = h2;
			invalidateRecursive();
		}
	} else if (name == "-relw") {
		double relw2 = value.getDouble();
		if (relw != relw2) {
			relw = relw2;
			invalidateRecursive();
		}
	} else if (name == "-relh") {
		double relh2 = value.getDouble();
		if (relh != relh2) {
			relh = relh2;
			invalidateRecursive();
		}
	} else if (name == "-scale") {
		double scale2 = value.getDouble();
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
		double size = value.getDouble();
		if (borderSize != size) {
			borderSize = size;
			invalidateLocal();
		}
	} else if (name == "-relbordersize") {
		double size = value.getDouble();
		if (relBorderSize != size) {
			relBorderSize = size;
			invalidateLocal();
		}
	} else if (name == "-borderrgba") {
		unsigned newRGBA = value.getInt();
		if (borderRGBA != newRGBA) {
			borderRGBA = newRGBA;
			invalidateLocal();
		}
	} else {
		OSDImageBasedWidget::setProperty(name, value);
	}
}

void OSDRectangle::getProperty(string_ref name, TclObject& result) const
{
	if (name == "-w") {
		result.setDouble(w);
	} else if (name == "-h") {
		result.setDouble(h);
	} else if (name == "-relw") {
		result.setDouble(relw);
	} else if (name == "-relh") {
		result.setDouble(relh);
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
	return (w    == 0.0) && (h    == 0.0) &&
	       (relw == 0.0) && (relh == 0.0);
}

void OSDRectangle::getWidthHeight(const OutputRectangle& output,
                                  double& width, double& height) const
{
	if (!imageName.empty() && image.get() && takeImageDimensions()) {
		width  = image->getWidth();
		height = image->getHeight();
	} else {
		double factor = getScaleFactor(output) * scale;
		width  = factor * w;
		height = factor * h;

		double pwidth, pheight;
		getParent()->getWidthHeight(output, pwidth, pheight);
		width  += pwidth  * relw;
		height += pheight * relh;
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
		    (((borderRGBA & 0xFF) == 0) || (borderSize == 0.0))) {
			// optimization: Sometimes it's useful to have a
			//   rectangle that will never be drawn, it only exists
			//   as a parent for sub-widgets. For those cases
			//   creating an IMAGE only wastes memory. So postpone
			//   creating it till alpha changes.
			return nullptr;
		}
		double width, height;
		getWidthHeight(output, width, height);
		int sw = int(round(width));
		int sh = int(round(height));
		double factor = getScaleFactor(output) * scale;
		int bs = int(round(factor * borderSize + width * relBorderSize));
		assert(bs >= 0);
		return make_unique<IMAGE>(sw, sh, getRGBA4(), bs, borderRGBA);
	} else {
		SystemFileContext context;
		string file = context.resolve(imageName);
		if (takeImageDimensions()) {
			double factor = getScaleFactor(output) * scale;
			return make_unique<IMAGE>(file, factor);
		} else {
			double width, height;
			getWidthHeight(output, width, height);
			int sw = int(round(width));
			int sh = int(round(height));
			return make_unique<IMAGE>(file, sw, sh);
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
