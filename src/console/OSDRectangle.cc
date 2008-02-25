// $Id$

#include "OSDRectangle.hh"
#include "OutputSurface.hh"
#include "SDLImage.hh"
#include "FileContext.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "components.hh"
#ifdef COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;
using std::set;

namespace openmsx {

OSDRectangle::OSDRectangle(OSDGUI& gui)
	: OSDWidget(gui)
	, x(0), y(0), w(0), h(0)
{
}

void OSDRectangle::getProperties(set<string>& result) const
{
	result.insert("-x");
	result.insert("-y");
	result.insert("-w");
	result.insert("-h");
	result.insert("-image");
	OSDWidget::getProperties(result);
}

void OSDRectangle::setProperty(const string& name, const string& value)
{
	if (name == "-x") {
		x = StringOp::stringToInt(value);
	} else if (name == "-y") {
		y = StringOp::stringToInt(value);
	} else if (name == "-w") {
		w = StringOp::stringToInt(value);
		invalidate();
	} else if (name == "-h") {
		h = StringOp::stringToInt(value);
		invalidate();
	} else if (name == "-rgba") {
		if (setRGBA(value) && imageName.empty()) {
			invalidate();
		}
	} else if (name == "-rgb") {
		if (setRGB(value) && imageName.empty()) {
			invalidate();
		}
	} else if (name == "-alpha") {
		if (setAlpha(value) && imageName.empty()) {
			invalidate();
		}
	} else if (name == "-image") {
		imageName = value;
		invalidate();
	} else {
		OSDWidget::setProperty(name, value);
	}
}

std::string OSDRectangle::getProperty(const string& name) const
{
	if (name == "-x") {
		return StringOp::toString(x);
	} else if (name == "-y") {
		return StringOp::toString(y);
	} else if (name == "-w") {
		return StringOp::toString(w);
	} else if (name == "-h") {
		return StringOp::toString(h);
	} else if (name == "-image") {
		return imageName;
	} else {
		return OSDWidget::getProperty(name);
	}
}

std::string OSDRectangle::getType() const
{
	return "rectangle";
}

template <typename IMAGE> void OSDRectangle::paint(OutputSurface& output)
{
	if (!image.get()) {
		try {
			if (imageName.empty()) {
				image.reset(new IMAGE(
					output, w, h, getAlpha(),
					getRed(), getGreen(), getBlue()));
			} else {
				SystemFileContext context;
				string file = context.resolve(imageName);
				if (w && h) {
					image.reset(new IMAGE(output, file, w, h));
				} else {
					image.reset(new IMAGE(output, file));
				}
			}
		} catch (MSXException& e) {
			// TODO print warning?
		}
	}
	if (image.get()) {
		byte alpha = imageName.empty() ? 255 : getAlpha();
		image->draw(x, y, alpha);
	}
}

void OSDRectangle::paintSDL(OutputSurface& output)
{
	paint<SDLImage>(output);
}

void OSDRectangle::paintGL(OutputSurface& output)
{
#ifdef COMPONENT_GL
	paint<GLImage>(output);
#endif
}

void OSDRectangle::invalidate()
{
	image.reset();
}

} // namespace openmsx
