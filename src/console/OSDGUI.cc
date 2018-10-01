#include "OSDGUI.hh"
#include "OSDWidget.hh"
#include "OSDRectangle.hh"
#include "OSDText.hh"
#include "Display.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "outer.hh"
#include <algorithm>
#include <cassert>
#include <memory>

using std::string;
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

void OSDGUI::OSDCommand::execute(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw SyntaxError();
	}
	auto& gui = OUTER(OSDGUI, osdCommand);
	string_view subCommand = tokens[1].getString();
	if (subCommand == "create") {
		create(tokens, result);
		gui.refresh();
	} else if (subCommand == "destroy") {
		destroy(tokens, result);
		gui.refresh();
	} else if (subCommand == "info") {
		info(tokens, result);
	} else if (subCommand == "exists") {
		exists(tokens, result);
	} else if (subCommand == "configure") {
		configure(tokens, result);
		gui.refresh();
	} else {
		throw CommandException(
			"Invalid subcommand '", subCommand, "', expected "
			"'create', 'destroy', 'info', 'exists' or 'configure'.");
	}
}

void OSDGUI::OSDCommand::create(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 4) {
		throw SyntaxError();
	}
	string_view type = tokens[2].getString();
	auto& fullname = tokens[3];
	auto fullnameStr = fullname.getString();

	auto& gui = OUTER(OSDGUI, osdCommand);
	auto& top = gui.getTopWidget();
	if (top.findByName(fullnameStr)) {
		throw CommandException(
			"There already exists a widget with this name: ",
			fullnameStr);
	}

	string_view parentname, childName;
	StringOp::splitOnLast(fullnameStr, '.', parentname, childName);
	auto* parent = childName.empty() ? &top : top.findByName(parentname);
	if (!parent) {
		throw CommandException(
			"Parent widget doesn't exist yet:", parentname);
	}

	auto widget = create(type, fullname);
	configure(*widget, tokens, 4);
	top.addName(*widget);
	parent->addWidget(std::move(widget));

	result = fullname;
}

unique_ptr<OSDWidget> OSDGUI::OSDCommand::create(
	string_view type, const TclObject& newName) const
{
	auto& gui = OUTER(OSDGUI, osdCommand);
	if (type == "rectangle") {
		return std::make_unique<OSDRectangle>(gui.display, newName);
	} else if (type == "text") {
		return std::make_unique<OSDText>(gui.display, newName);
	} else {
		throw CommandException(
			"Invalid widget type '", type, "', expected "
			"'rectangle' or 'text'.");
	}
}

void OSDGUI::OSDCommand::destroy(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	auto fullname = tokens[2].getString();

	auto& gui = OUTER(OSDGUI, osdCommand);
	auto& top = gui.getTopWidget();
	auto* widget = top.findByName(fullname);
	if (!widget) {
		// widget not found, not an error
		result.setBoolean(false);
		return;
	}

	auto* parent = widget->getParent();
	if (!parent) {
		throw CommandException("Can't destroy the top widget.");
	}

	top.removeName(*widget);
	parent->deleteWidget(*widget);
	result.setBoolean(true);
}

void OSDGUI::OSDCommand::info(array_ref<TclObject> tokens, TclObject& result)
{
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
	default:
		throw SyntaxError();
	}
}

void OSDGUI::OSDCommand::exists(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	auto& gui = OUTER(OSDGUI, osdCommand);
	auto* widget = gui.getTopWidget().findByName(tokens[2].getString());
	result.setBoolean(widget != nullptr);
}

void OSDGUI::OSDCommand::configure(array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() < 3) {
		throw SyntaxError();
	}
	configure(getWidget(tokens[2].getString()), tokens, 3);
}

void OSDGUI::OSDCommand::configure(OSDWidget& widget, array_ref<TclObject> tokens,
                                   unsigned skip)
{
	assert(tokens.size() >= skip);
	if ((tokens.size() - skip) & 1) {
		// odd number of extra arguments
		throw CommandException(
			"Missing value for '", tokens.back().getString(), "'.");
	}

	auto& interp = getInterpreter();
	for (size_t i = skip; i < tokens.size(); i += 2) {
		const auto& propName = tokens[i + 0].getString();
		widget.setProperty(interp, propName, tokens[i + 1]);
	}
}

string OSDGUI::OSDCommand::help(const vector<string>& tokens) const
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
	auto& gui = OUTER(OSDGUI, osdCommand);
	if (tokens.size() == 2) {
		static const char* const cmds[] = {
			"create", "destroy", "info", "exists", "configure"
		};
		completeString(tokens, cmds);
	} else if ((tokens.size() == 3) && (tokens[1] == "create")) {
		static const char* const types[] = { "rectangle", "text" };
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
			} else if ((tokens[1] == "configure") ||
			           (tokens[1] == "info")) {
				const auto& widget = getWidget(tokens[2]);
				properties = widget.getProperties();
			}
			completeString(tokens, properties);
		} catch (MSXException&) {
			// ignore
		}
	}
}

OSDWidget& OSDGUI::OSDCommand::getWidget(string_view widgetName) const
{
	auto& gui = OUTER(OSDGUI, osdCommand);
	auto* widget = gui.getTopWidget().findByName(widgetName);
	if (!widget) {
		throw CommandException("No widget with name ", widgetName);
	}
	return *widget;
}

} // namespace openmsx
