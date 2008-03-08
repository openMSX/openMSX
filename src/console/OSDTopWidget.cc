// $Id$

#include "OSDTopWidget.hh"
#include "OutputSurface.hh"

namespace openmsx {

OSDTopWidget::OSDTopWidget()
	: OSDWidget("")
{
}

std::string OSDTopWidget::getType() const
{
	return "top";
}

void OSDTopWidget::getWidthHeight(const OutputSurface& output,
                                  int& width, int& height) const
{
	width  = output.getWidth();
	height = output.getHeight();
}

void OSDTopWidget::invalidateLocal()
{
	// nothing
}

void OSDTopWidget::paintSDL(OutputSurface& /*output*/)
{
	// nothing
}

void OSDTopWidget::paintGL (OutputSurface& /*output*/)
{
	// nothing
}

} // namespace openmsx
