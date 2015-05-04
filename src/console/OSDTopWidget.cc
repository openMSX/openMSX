#include "OSDTopWidget.hh"
#include "OSDGUI.hh"
#include "OutputRectangle.hh"
#include "Display.hh"
#include "CliComm.hh"

namespace openmsx {

OSDTopWidget::OSDTopWidget(OSDGUI& gui_)
	: OSDWidget("")
	, gui(gui_)
{
}

string_ref OSDTopWidget::getType() const
{
	return "top";
}

void OSDTopWidget::getWidthHeight(const OutputRectangle& output,
                                  float& width, float& height) const
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

void OSDTopWidget::queueError(std::string message)
{
	errors.push_back(std::move(message));
}

void OSDTopWidget::showAllErrors()
{
	auto& cliComm = gui.getDisplay().getCliComm();
	for (const auto& message : errors) {
		cliComm.printWarning(std::move(message));
	}
	errors.clear();
}

} // namespace openmsx
