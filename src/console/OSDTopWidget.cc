// $Id$

#include "OSDTopWidget.hh"
#include "OutputRectangle.hh"

namespace openmsx {

OSDTopWidget::OSDTopWidget()
	: OSDWidget("")
{
}

string_ref OSDTopWidget::getType() const
{
	return "top";
}

void OSDTopWidget::getWidthHeight(const OutputRectangle& output,
                                  double& width, double& height) const
{
	width  = output.getOutputWidth();
	height = output.getOutputHeight();
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
