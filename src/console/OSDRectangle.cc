// $Id$

#include "OSDRectangle.hh"
#include "SDLImage.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include "components.hh"
#include <cassert>
#ifdef COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;
using std::set;

namespace openmsx {

OSDRectangle::OSDRectangle(const OSDGUI& gui, const string& name)
	: OSDImageBasedWidget(gui, name)
	, w(0.0), h(0.0), scale(1.0)
{
}

void OSDRectangle::getProperties(set<string>& result) const
{
	result.insert("-w");
	result.insert("-h");
	result.insert("-scale");
	result.insert("-image");
	OSDImageBasedWidget::getProperties(result);
}

void OSDRectangle::setProperty(const string& name, const string& value)
{
	if (name == "-w") {
		w = StringOp::stringToDouble(value);
		invalidateRecursive();
	} else if (name == "-h") {
		h = StringOp::stringToDouble(value);
		invalidateRecursive();
	} else if (name == "-scale") {
		scale = StringOp::stringToDouble(value);
		invalidateRecursive();
	} else if (name == "-image") {
		if (!value.empty() && !FileOperations::isRegularFile(value)) {
			throw CommandException("Not a valid image file: " + value);
		}
		imageName = value;
		invalidateRecursive();
	} else {
		OSDImageBasedWidget::setProperty(name, value);
	}
}

std::string OSDRectangle::getProperty(const string& name) const
{
	if (name == "-w") {
		return StringOp::toString(w);
	} else if (name == "-h") {
		return StringOp::toString(h);
	} else if (name == "-scale") {
		return StringOp::toString(scale);
	} else if (name == "-image") {
		return imageName;
	} else {
		return OSDImageBasedWidget::getProperty(name);
	}
}

std::string OSDRectangle::getType() const
{
	return "rectangle";
}

void OSDRectangle::getWidthHeight(const OutputSurface& output,
                                  double& width, double& height) const
{
	if (image.get()) {
		OSDImageBasedWidget::getWidthHeight(output, width, height);
	} else {
		// No image allocated, must be either because of
		//  - an error: in this case we can still do better than the
		//              implementation in the base class
		//  - the alpha=0 optimization
		//  - calculation of clip rectangle when image was not yet
		//    allocated (new widget or invalidated)
		double factor = getScaleFactor(output) * scale;
		width  = factor * w;
		height = factor * h;
	}
}

template <typename IMAGE> BaseImage* OSDRectangle::create(
	OutputSurface& output)
{
	double factor = getScaleFactor(output) * scale;
	int sw = int(factor * w);
	int sh = int(factor * h);
	if (imageName.empty()) {
		if (getAlpha()) {
			// note: Image is create with alpha = 255. Actual
			//  alpha is applied during drawing. This way we
			//  can also reuse the same image if only alpha
			//  changes.
			return new IMAGE(output, sw, sh, 255,
			                 getRed(), getGreen(), getBlue());
		} else {
			// optimization: Sometimes it's useful to have a
			//   rectangle that will never be drawn, it only exists
			//   as a parent for sub-widgets. For those cases
			//   creating an IMAGE only wastes memory. So postpone
			//   creating it till alpha changes.
			return NULL;
		}
	} else {
		SystemFileContext context;
		CommandController* controller = NULL; // ok for SystemFileContext
		string file = context.resolve(*controller, imageName);
		if (sw && sh) {
			return new IMAGE(output, file, sw, sh);
		} else {
			return new IMAGE(output, file, factor);
		}
	}
}

BaseImage* OSDRectangle::createSDL(OutputSurface& output)
{
	return create<SDLImage>(output);
}

BaseImage* OSDRectangle::createGL(OutputSurface& output)
{
#ifdef COMPONENT_GL
	return create<GLImage>(output);
#else
	(void)&output;
	return NULL;
#endif
}

} // namespace openmsx
