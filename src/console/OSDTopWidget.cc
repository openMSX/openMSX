#include "OSDTopWidget.hh"
#include "OSDGUI.hh"
#include "OutputRectangle.hh"
#include "Display.hh"
#include "CliComm.hh"
#include "KeyRange.hh"

namespace openmsx {

OSDTopWidget::OSDTopWidget(OSDGUI& gui_)
	: OSDWidget(TclObject())
	, gui(gui_)
{
	addName(*this);
}

string_ref OSDTopWidget::getType() const
{
	return "top";
}

gl::vec2 OSDTopWidget::getSize(const OutputRectangle& output) const
{
	return gl::vec2(output.getOutputWidth(),
	                output.getOutputHeight());
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

OSDWidget* OSDTopWidget::findByName(string_ref widgetName)
{
	auto it = widgetsByName.find(widgetName);
	return (it != end(widgetsByName)) ? *it : nullptr;
}

const OSDWidget* OSDTopWidget::findByName(string_ref widgetName) const
{
	return const_cast<OSDTopWidget*>(this)->findByName(widgetName);
}

void OSDTopWidget::addName(OSDWidget& widget)
{
	assert(!widgetsByName.contains(widget.getName()));
	widgetsByName.emplace_noDuplicateCheck(&widget);
}

void OSDTopWidget::removeName(OSDWidget& widget)
{
	auto it = widgetsByName.find(widget.getName());
	assert(it != end(widgetsByName));
	for (auto& child : (*it)->getChildren()) {
		removeName(*child);
	}
	widgetsByName.erase(it);
}

std::vector<string_ref> OSDTopWidget::getAllWidgetNames() const
{
	std::vector<string_ref> result;
	for (auto* p : widgetsByName) {
		result.push_back(p->getName());
	}
	return result;
}

} // namespace openmsx
