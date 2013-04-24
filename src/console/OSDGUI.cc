#include "OSDGUI.hh"
#include "OSDWidget.hh"
#include "OSDTopWidget.hh"
#include "OSDRectangle.hh"
#include "OSDText.hh"
#include "Display.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "openmsx.hh"
#include "memory.hh"
#include <algorithm>
#include <cassert>

using std::string;
using std::shared_ptr;
using std::vector;

namespace openmsx {

class OSDCommand : public Command
{
public:
	OSDCommand(OSDGUI& gui, CommandController& commandController);
	virtual void execute(const vector<TclObject>& tokens, TclObject& result);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;

private:
	void create   (const vector<TclObject>& tokens, TclObject& result);
	void destroy  (const vector<TclObject>& tokens, TclObject& result);
	void info     (const vector<TclObject>& tokens, TclObject& result);
	void exists   (const vector<TclObject>& tokens, TclObject& result);
	void configure(const vector<TclObject>& tokens, TclObject& result);
	shared_ptr<OSDWidget> create(string_ref type, const string& name) const;
	void configure(OSDWidget& widget, const vector<TclObject>& tokens,
	               unsigned skip);

	OSDWidget& getWidget(string_ref name) const;

	OSDGUI& gui;
};


// class OSDGUI

OSDGUI::OSDGUI(CommandController& commandController, Display& display_)
	: display(display_)
	, osdCommand(make_unique<OSDCommand>(*this, commandController))
	, topWidget(make_unique<OSDTopWidget>())
{
}

OSDGUI::~OSDGUI()
{
}

Display& OSDGUI::getDisplay() const
{
	return display;
}

OSDWidget& OSDGUI::getTopWidget() const
{
	return *topWidget;
}

void OSDGUI::refresh() const
{
	getDisplay().repaintDelayed(40000); // 25 fps
}


// class OSDCommand

OSDCommand::OSDCommand(OSDGUI& gui_, CommandController& commandController)
	: Command(commandController, "osd")
	, gui(gui_)
{
}

void OSDCommand::execute(const vector<TclObject>& tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw SyntaxError();
	}
	string_ref subCommand = tokens[1].getString();
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
			"Invalid subcommand '" + subCommand + "', expected "
			"'create', 'destroy', 'info', 'exists' or 'configure'.");
	}
}

void OSDCommand::create(const vector<TclObject>& tokens, TclObject& result)
{
	if (tokens.size() < 4) {
		throw SyntaxError();
	}
	string_ref type = tokens[2].getString();
	string_ref fullname = tokens[3].getString();
	string_ref parentname, name;
	StringOp::splitOnLast(fullname, '.', parentname, name);
	if (name.empty()) std::swap(parentname, name);

	OSDWidget* parent = gui.getTopWidget().findSubWidget(parentname);
	if (!parent) {
		throw CommandException(
			"Parent widget doesn't exist yet:" + parentname);
	}
	if (parent->findSubWidget(name)) {
		throw CommandException(
			"There already exists a widget with this name: " +
			fullname);
	}

	shared_ptr<OSDWidget> widget = create(type, name.str());
	configure(*widget, tokens, 4);
	parent->addWidget(widget);

	result.setString(fullname);
}

shared_ptr<OSDWidget> OSDCommand::create(
		string_ref type, const string& name) const
{
	if (type == "rectangle") {
		return std::make_shared<OSDRectangle>(gui, name);
	} else if (type == "text") {
		return std::make_shared<OSDText>(gui, name);
	} else {
		throw CommandException(
			"Invalid widget type '" + type + "', expected "
			"'rectangle' or 'text'.");
	}
}

void OSDCommand::destroy(const vector<TclObject>& tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	OSDWidget* widget = gui.getTopWidget().findSubWidget(tokens[2].getString());
	if (!widget) {
		// widget not found, not an error
		result.setBoolean(false);
		return;
	}
	OSDWidget* parent = widget->getParent();
	if (!parent) {
		throw CommandException("Can't destroy the top widget.");
	}
	parent->deleteWidget(*widget);
	result.setBoolean(true);
}

void OSDCommand::info(const vector<TclObject>& tokens, TclObject& result)
{
	switch (tokens.size()) {
	case 2: {
		// list widget names
		vector<string> names;
		gui.getTopWidget().listWidgetNames("", names);
		result.addListElements(names);
		break;
	}
	case 3: {
		// list properties for given widget
		const OSDWidget& widget = getWidget(tokens[2].getString());
		result.addListElements(widget.getProperties());
		break;
	}
	case 4: {
		// get current value for given widget/property
		const OSDWidget& widget = getWidget(tokens[2].getString());
		widget.getProperty(tokens[3].getString(), result);
		break;
	}
	default:
		throw SyntaxError();
	}
}

void OSDCommand::exists(const vector<TclObject>& tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	OSDWidget* widget = gui.getTopWidget().findSubWidget(tokens[2].getString());
	result.setBoolean(widget != nullptr);
}

void OSDCommand::configure(const vector<TclObject>& tokens, TclObject& /*result*/)
{
	if (tokens.size() < 3) {
		throw SyntaxError();
	}
	configure(getWidget(tokens[2].getString()), tokens, 3);
}

void OSDCommand::configure(OSDWidget& widget, const vector<TclObject>& tokens,
                           unsigned skip)
{
	assert(tokens.size() >= skip);
	if ((tokens.size() - skip) & 1) {
		// odd number of extra arguments
		throw CommandException(
			"Missing value for '" + tokens.back().getString() + "'.");
	}

	for (unsigned i = skip; i < tokens.size(); i += 2) {
		string_ref name  = tokens[i + 0].getString();
		widget.setProperty(name, tokens[i + 1]);
	}
}

string OSDCommand::help(const vector<string>& tokens) const
{
	if (tokens.size() >= 2) {
		if (tokens[1] == "create") {
			return
			  "osd create <type> <widget-path> [<property-name> <property-value>]...\n"
			  "\n"
			  "Creates a new OSD widget of given type. Path is a "
			  "hierarchical name for the wiget (separated by '.')."
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

void OSDCommand::tabCompletion(vector<string>& tokens) const
{
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
		vector<string> names;
		gui.getTopWidget().listWidgetNames("", names);
		completeString(tokens, names);
	} else {
		try {
			vector<string_ref> properties;
			if (tokens[1] == "create") {
				shared_ptr<OSDWidget> widget = create(tokens[2], "");
				properties = widget->getProperties();
			} else if ((tokens[1] == "configure") ||
			           (tokens[1] == "info")) {
				const OSDWidget& widget = getWidget(tokens[2]);
				properties = widget.getProperties();
			}
			completeString(tokens, properties);
		} catch (MSXException&) {
			// ignore
		}
	}
}

OSDWidget& OSDCommand::getWidget(string_ref name) const
{
	OSDWidget* widget = gui.getTopWidget().findSubWidget(name);
	if (!widget) {
		throw CommandException("No widget with name " + name);
	}
	return *widget;
}

} // namespace openmsx
