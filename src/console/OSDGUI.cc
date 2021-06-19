#include "OSDGUI.hh"
#include "OSDWidget.hh"
#include "OSDRectangle.hh"
#include "OSDText.hh"
#include "Display.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "one_of.hh"
#include "outer.hh"
#include <memory>
#include <utility>

using std::string;
using std::string_view;
using std::unique_ptr;
using std::vector;

namespace openmsx {

// class OSDGUI

OSDGUI::OSDGUI(CommandController& commandController, Display& display_)
	: display(display_)
	, osdCommand(commandController)
	, topWidget(display_)
{
}

void OSDGUI::refresh() const
{
	getDisplay().repaintDelayed(40000); // 25 fps
}


// class OSDCommand

OSDGUI::OSDCommand::OSDCommand(CommandController& commandController_)
	: Command(commandController_, "osd")
{
}

void OSDGUI::OSDCommand::execute(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{2}, "subcommand ?arg ...?");
	executeSubCommand(tokens[1].getString(),
		"create",    [&]{ create(tokens, result); },
		"destroy",   [&]{ destroy(tokens, result); },
		"info",      [&]{ info(tokens, result); },
		"exists",    [&]{ exists(tokens, result); },
		"configure", [&]{ configure(tokens, result); });
}

void OSDGUI::OSDCommand::create(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{4}, Prefix{2}, "type name ?property value ...?");
	string_view type = tokens[2].getString();
	const auto& fullname = tokens[3];
	auto fullnameStr = fullname.getString();

	auto& gui = OUTER(OSDGUI, osdCommand);
	auto& top = gui.getTopWidget();
	if (top.findByName(fullnameStr)) {
		throw CommandException(
			"There already exists a widget with this name: ",
			fullnameStr);
	}

	auto [parentname, childName] = StringOp::splitOnLast(fullnameStr, '.');
	auto* parent = childName.empty() ? &top : top.findByName(parentname);
	if (!parent) {
		throw CommandException(
			"Parent widget doesn't exist yet:", parentname);
	}

	auto widget = create(type, fullname);
	auto* widget2 = widget.get();
	configure(*widget, tokens.subspan(4));
	top.addName(*widget);
	parent->addWidget(std::move(widget));

	result = fullname;
	if (widget2->isVisible()) {
		gui.refresh();
	}
}

unique_ptr<OSDWidget> OSDGUI::OSDCommand::create(
	string_view type, const TclObject& name) const
{
	auto& gui = OUTER(OSDGUI, osdCommand);
	if (type == "rectangle") {
		return std::make_unique<OSDRectangle>(gui.display, name);
	} else if (type == "text") {
		return std::make_unique<OSDText>(gui.display, name);
	} else {
		throw CommandException(
			"Invalid widget type '", type, "', expected "
			"'rectangle' or 'text'.");
	}
}

void OSDGUI::OSDCommand::destroy(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 3, "name");
	auto fullname = tokens[2].getString();

	auto& gui = OUTER(OSDGUI, osdCommand);
	auto& top = gui.getTopWidget();
	auto* widget = top.findByName(fullname);
	if (!widget) {
		// widget not found, not an error
		result = false;
		return;
	}

	auto* parent = widget->getParent();
	if (!parent) {
		throw CommandException("Can't destroy the top widget.");
	}

	if (widget->isVisible()) {
		gui.refresh();
	}
	top.removeName(*widget);
	parent->deleteWidget(*widget);
	result = true;
}

void OSDGUI::OSDCommand::info(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, Between{2, 4}, Prefix{2}, "?name? ?property?");
	auto& gui = OUTER(OSDGUI, osdCommand);
	switch (tokens.size()) {
	case 2: {
		// list widget names
		result.addListElements(gui.getTopWidget().getAllWidgetNames());
		break;
	}
	case 3: {
		// list properties for given widget
		const auto& widget = getWidget(tokens[2].getString());
		result.addListElements(widget.getProperties());
		break;
	}
	case 4: {
		// get current value for given widget/property
		const auto& widget = getWidget(tokens[2].getString());
		widget.getProperty(tokens[3].getString(), result);
		break;
	}
	}
}

void OSDGUI::OSDCommand::exists(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 3, "name");
	auto& gui = OUTER(OSDGUI, osdCommand);
	auto* widget = gui.getTopWidget().findByName(tokens[2].getString());
	result = widget != nullptr;
}

void OSDGUI::OSDCommand::configure(span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, AtLeast{3}, "name ?property value ...?");
	auto& widget = getWidget(tokens[2].getString());
	configure(widget, tokens.subspan(3));
	if (widget.isVisible()) {
		auto& gui = OUTER(OSDGUI, osdCommand);
		gui.refresh();
	}
}

void OSDGUI::OSDCommand::configure(OSDWidget& widget, span<const TclObject> tokens)
{
	if (tokens.size() & 1) {
		// odd number of extra arguments
		throw CommandException(
			"Missing value for '", tokens.back().getString(), "'.");
	}

	auto& interp = getInterpreter();
	for (size_t i = 0; i < tokens.size(); i += 2) {
		const auto& propName = tokens[i + 0].getString();
		widget.setProperty(interp, propName, tokens[i + 1]);
	}
}

string OSDGUI::OSDCommand::help(span<const TclObject> tokens) const
{
	if (tokens.size() >= 2) {
		if (tokens[1] == "create") {
			return
			  "osd create <type> <widget-path> [<property-name> <property-value>]...\n"
			  "\n"
			  "Creates a new OSD widget of given type. Path is a "
			  "hierarchical name for the widget (separated by '.'). "
			  "The parent widget for this new widget must already "
			  "exist.\n"
			  "Optionally you can set initial values for one or "
			  "more properties.\n"
			  "This command returns the path of the newly created "
			  "widget. This is path is again needed to configure "
			  "or to remove the widget. It may be useful to assign "
			  "this path to a variable.";
		} else if (tokens[1] == "destroy") {
			return
			  "osd destroy <widget-path>\n"
			  "\n"
			  "Remove the specified OSD widget. Returns '1' on "
			  "success and '0' when widget couldn't be destroyed "
			  "because there was no widget with that name";
		} else if (tokens[1] == "info") {
			return
			  "osd info [<widget-path> [<property-name>]]\n"
			  "\n"
			  "Query various information about the OSD status. "
			  "You can call this command with 0, 1 or 2 arguments.\n"
			  "Without any arguments, this command returns a list "
			  "of all existing widget IDs.\n"
			  "When a path is given as argument, this command "
			  "returns a list of available properties for that widget.\n"
			  "When both path and property name arguments are "
			  "given, this command returns the current value of "
			  "that property.";
		} else if (tokens[1] == "exists") {
			return
			  "osd exists <widget-path>\n"
			  "\n"
			  "Test whether there exists a widget with given name. "
			  "This subcommand is meant to be used in scripts.";
		} else if (tokens[1] == "configure") {
			return
			  "osd configure <widget-path> [<property-name> <property-value>]...\n"
			  "\n"
			  "Modify one or more properties on the given widget.";
		} else {
			return "No such subcommand, see 'help osd'.";
		}
	} else {
		return
		  "Low level OSD GUI commands\n"
		  "  osd create <type> <widget-path> [<property-name> <property-value>]...\n"
		  "  osd destroy <widget-path>\n"
		  "  osd info [<widget-path> [<property-name>]]\n"
		  "  osd exists <widget-path>\n"
		  "  osd configure <widget-path> [<property-name> <property-value>]...\n"
		  "Use 'help osd <subcommand>' to see more info on a specific subcommand";
	}
}

void OSDGUI::OSDCommand::tabCompletion(vector<string>& tokens) const
{
	using namespace std::literals;
	auto& gui = OUTER(OSDGUI, osdCommand);
	if (tokens.size() == 2) {
		static constexpr std::array cmds = {
			"create"sv, "destroy"sv, "info"sv, "exists"sv, "configure"sv
		};
		completeString(tokens, cmds);
	} else if ((tokens.size() == 3) && (tokens[1] == "create")) {
		static constexpr std::array types = {"rectangle"sv, "text"sv};
		completeString(tokens, types);
	} else if ((tokens.size() == 3) ||
	           ((tokens.size() == 4) && (tokens[1] == "create"))) {
		completeString(tokens, gui.getTopWidget().getAllWidgetNames());
	} else {
		try {
			vector<string_view> properties;
			if (tokens[1] == "create") {
				auto widget = create(tokens[2], TclObject());
				properties = widget->getProperties();
			} else if (tokens[1] == one_of("configure", "info")) {
				const auto& widget = getWidget(tokens[2]);
				properties = widget.getProperties();
			}
			completeString(tokens, properties);
		} catch (MSXException&) {
			// ignore
		}
	}
}

OSDWidget& OSDGUI::OSDCommand::getWidget(string_view name) const
{
	auto& gui = OUTER(OSDGUI, osdCommand);
	auto* widget = gui.getTopWidget().findByName(name);
	if (!widget) {
		throw CommandException("No widget with name ", name);
	}
	return *widget;
}

} // namespace openmsx
