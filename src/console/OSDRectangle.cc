// $Id$

#include "OSDRectangle.hh"
#include "SDLImage.hh"
#include "OutputSurface.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "components.hh"
#include "Math.hh"
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
{
}

void OSDRectangle::getProperties(set<string>& result) const
{
	result.insert("-w");
	result.insert("-h");
	result.insert("-relw");
	result.insert("-relh");
	result.insert("-scale");
	result.insert("-image");
	OSDImageBasedWidget::getProperties(result);
}

void OSDRectangle::setProperty(const string& name, const TclObject& value)
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
		string val = value.getString();
		if (imageName != val) {
			if (!val.empty() && !FileOperations::isRegularFile(val)) {
				throw CommandException("Not a valid image file: " + val);
			}
			imageName = val;
			invalidateRecursive();
		}
	} else {
		OSDImageBasedWidget::setProperty(name, value);
	}
}

void OSDRectangle::getProperty(const string& name, TclObject& result) const
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
	} else {
		OSDImageBasedWidget::getProperty(name, result);
	}
}

std::string OSDRectangle::getType() const
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

template <typename IMAGE> BaseImage* OSDRectangle::create(
	OutputSurface& output)
{
	if (imageName.empty()) {
		if (getAlpha()) {
			double width, height;
			getWidthHeight(output, width, height);
			int sw = int(round(width));
			int sh = int(round(height));
			// note: Image is create with alpha = 255. Actual
			//  alpha is applied during drawing. This way we
			//  can also reuse the same image if only alpha
			//  changes.
			return new IMAGE(sw, sh, 255,
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
		if (takeImageDimensions()) {
			double factor = getScaleFactor(output) * scale;
			return new IMAGE(file, factor);
		} else {
			double width, height;
			getWidthHeight(output, width, height);
			int sw = int(round(width));
			int sh = int(round(height));
			return new IMAGE(file, sw, sh);
		}
	}
}

BaseImage* OSDRectangle::createSDL(OutputSurface& output)
{
	return create<SDLImage>(output);
}

BaseImage* OSDRectangle::createGL(OutputSurface& output)
{
#if COMPONENT_GL
	return create<GLImage>(output);
#else
	(void)&output;
	return NULL;
#endif
}

} // namespace openmsx
