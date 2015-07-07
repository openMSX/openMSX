#include "OSDTopWidget.hh"
#include "OSDGUI.hh"
#include "OutputRectangle.hh"
#include "Display.hh"
#include "CliComm.hh"
#include "KeyRange.hh"

namespace openmsx {

OSDTopWidget::OSDTopWidget(OSDGUI& gui_)
	: gui(gui_)
{
	addName(TclObject(), *this);
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

OSDWidget* OSDTopWidget::findByName(string_ref name)
{
	auto it = widgetsByName.find(name);
	return (it != end(widgetsByName)) ? it->second : nullptr;
}

const OSDWidget* OSDTopWidget::findByName(string_ref name) const
{
	return const_cast<OSDTopWidget*>(this)->findByName(name);
}

void OSDTopWidget::addName(const TclObject& name, OSDWidget& widget)
{
	assert(!widgetsByName.contains(name));
	widgetsByName.emplace_noDuplicateCheck(name, &widget);
}

void OSDTopWidget::removeName(string_ref name)
{
	assert(widgetsByName.contains(name));
	widgetsByName.erase(name);
}

std::vector<string_ref> OSDTopWidget::getAllWidgetNames() const
{
	std::vector<string_ref> result;
	for (auto& p : widgetsByName) {
		result.push_back(p.first.getString());
	}
	return result;
}

} // namespace openmsx
