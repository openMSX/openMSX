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

void OSDTopWidget::transformXY(const OutputSurface& output,
                               int x, int y, double relx, double rely,
                               int& outx, int& outy) const
{
	outx = x + int(relx * output.getWidth());
	outy = y + int(rely * output.getHeight());
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
