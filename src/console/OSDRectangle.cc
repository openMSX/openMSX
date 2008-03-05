// $Id$

#include "OSDRectangle.hh"
#include "SDLImage.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include "components.hh"
#ifdef COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;
using std::set;

namespace openmsx {

OSDRectangle::OSDRectangle(const OSDGUI& gui, const string& name)
	: OSDImageBasedWidget(gui, name)
	, w(0), h(0)
{
}

void OSDRectangle::getProperties(set<string>& result) const
{
	result.insert("-w");
	result.insert("-h");
	result.insert("-image");
	OSDImageBasedWidget::getProperties(result);
}

void OSDRectangle::setProperty(const string& name, const string& value)
{
	if (name == "-w") {
		w = StringOp::stringToInt(value);
		invalidateRecursive();
	} else if (name == "-h") {
		h = StringOp::stringToInt(value);
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

template <typename IMAGE> BaseImage* OSDRectangle::create(
	OutputSurface& output)
{
	int factor = getScaleFactor(output);
	int sw = factor * w;
	int sh = factor * h;
	if (imageName.empty()) {
		return new IMAGE(output, sw, sh, 255,
		                 getRed(), getGreen(), getBlue());
	} else {
		SystemFileContext context;
		string file = context.resolve(imageName);
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
#endif
	(void)output;
	return NULL;
}

} // namespace openmsx
