#include "OSDTopWidget.hh"

#include "CliComm.hh"
#include "Display.hh"
#include "OutputSurface.hh"

namespace openmsx {

OSDTopWidget::OSDTopWidget(Display& display_)
	: OSDWidget(display_, TclObject())
{
	addName(*this);
}

std::string_view OSDTopWidget::getType() const
{
	return "top";
}

gl::vec2 OSDTopWidget::getSize(const OutputSurface& output) const
{
	return gl::vec2(output.getLogicalSize()); // int -> float
}

bool OSDTopWidget::isVisible() const
{
	return false;
}

bool OSDTopWidget::isRecursiveFading() const
{
	return false; // not fading
}

void OSDTopWidget::invalidateLocal()
{
	// nothing
}

void OSDTopWidget::paint(OutputSurface& /*output*/)
{
	// nothing
}

void OSDTopWidget::queueError(std::string message)
{
	errors.push_back(std::move(message));
}

void OSDTopWidget::showAllErrors()
{
	auto& cliComm = getDisplay().getCliComm();
	for (auto& message : errors) {
		cliComm.printWarning(std::move(message));
	}
	errors.clear();
}

OSDWidget* OSDTopWidget::findByName(std::string_view widgetName)
{
	auto it = widgetsByName.find(widgetName);
	return (it != end(widgetsByName)) ? *it : nullptr;
}

const OSDWidget* OSDTopWidget::findByName(std::string_view widgetName) const
{
	auto it = widgetsByName.find(widgetName);
	return (it != end(widgetsByName)) ? *it : nullptr;
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

} // namespace openmsx
