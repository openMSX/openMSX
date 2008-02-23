// $Id$

#include "OSDGUI.hh"
#include "OSDWidget.hh"
#include "OSDRectangle.hh"
#include "OSDText.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include <algorithm>
#include <cassert>

using std::string;
using std::set;
using std::vector;
using std::auto_ptr;

namespace openmsx {

class OSDCommand : public Command
{
public:
	OSDCommand(OSDGUI& gui, CommandController& commandController);
	virtual void execute(const vector<TclObject*>& tokens, TclObject& result);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;

private:
	void create   (const vector<TclObject*>& tokens, TclObject& result);
	void destroy  (const vector<TclObject*>& tokens, TclObject& result);
	void info     (const vector<TclObject*>& tokens, TclObject& result);
	void configure(const vector<TclObject*>& tokens, TclObject& result);
	auto_ptr<OSDWidget> create(const string& type) const;
	void configure(OSDWidget& widget, const vector<TclObject*>& tokens);

	OSDWidget& getWidget(const string& name) const;

	OSDGUI& gui;
};


// class OSDGUI

OSDGUI::OSDGUI(CommandController& commandController, Display& display_)
	: display(display_)
	, osdCommand(new OSDCommand(*this, commandController))
{
}

OSDGUI::~OSDGUI()
{
	for (Widgets::const_iterator it = widgets.begin();
	     it != widgets.end(); ++it) {
		delete *it;
	}
}

const OSDGUI::Widgets& OSDGUI::getWidgets() const
{
	return widgets;
}

void OSDGUI::addWidget(auto_ptr<OSDWidget> widget)
{
	widgets.push_back(widget.release());
	resort();
}

void OSDGUI::deleteWidget(OSDWidget& widget)
{
	for (Widgets::iterator it = widgets.begin();
	     it != widgets.end(); ++it) {
		if (*it == &widget) {
			delete *it;
			widgets.erase(it);
			return;
		}
	}
	assert(false);
}

struct AscendingZ {
	bool operator()(const OSDWidget* lhs, const OSDWidget* rhs) const {
		return lhs->getZ() < rhs->getZ();
	}
};
void OSDGUI::resort()
{
	std::stable_sort(widgets.begin(), widgets.end(), AscendingZ());
}

Display& OSDGUI::getDisplay() const
{
	return display;
}


// class OSDCommand

OSDCommand::OSDCommand(OSDGUI& gui_, CommandController& commandController)
	: Command(commandController, "osd")
	, gui(gui_)
{
}

void OSDCommand::execute(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw SyntaxError();
	}
	string subCommand = tokens[1]->getString();
	if (subCommand == "create") {
		create(tokens, result);
	} else if (subCommand == "destroy") {
		destroy(tokens, result);
	} else if (subCommand == "info") {
		info(tokens, result);
	} else if (subCommand == "configure") {
		configure(tokens, result);
	} else {
		throw CommandException(
			"Invalid subcommand '" + subCommand + "', expected "
			"'create', 'destroy', 'info' or 'configure'.");
	}
}

void OSDCommand::create(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() < 3) {
		throw SyntaxError();
	}
	auto_ptr<OSDWidget> widget = create(tokens[2]->getString());
	configure(*widget, tokens);

	result.setString(widget->getName());
	gui.addWidget(widget);
}

auto_ptr<OSDWidget> OSDCommand::create(const string& type) const
{
	if (type == "rectangle") {
		return auto_ptr<OSDWidget>(new OSDRectangle(gui));
	} else if (type == "text") {
		return auto_ptr<OSDWidget>(new OSDText(gui));
	} else {
		throw CommandException(
			"Invalid widget type '" + type + "', expected "
			"'rectangle' or 'text'.");
	}
}

void OSDCommand::destroy(const vector<TclObject*>& tokens, TclObject& /*result*/)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	// note: iterates twice over list, but we don't care for now
	gui.deleteWidget(getWidget(tokens[2]->getString()));
}

void OSDCommand::info(const vector<TclObject*>& tokens, TclObject& result)
{
	switch (tokens.size()) {
	case 2: {
		// list widget names
		const OSDGUI::Widgets& widgets = gui.getWidgets();
		for (OSDGUI::Widgets::const_iterator it = widgets.begin();
		     it != widgets.end(); ++it) {
			result.addListElement((*it)->getName());
		}
		break;
	}
	case 3: {
		// list properties for given widget
		const OSDWidget& widget = getWidget(tokens[2]->getString());
		set<string> properties;
		widget.getProperties(properties);
		result.addListElements(properties.begin(), properties.end());
		break;
	}
	case 4: {
		// get current value for given widget/property
		const OSDWidget& widget = getWidget(tokens[2]->getString());
		result.setString(widget.getProperty(tokens[3]->getString()));
		break;
	}
	default:
		throw SyntaxError();
	}
}

void OSDCommand::configure(const vector<TclObject*>& tokens, TclObject& /*result*/)
{
	if (tokens.size() < 3) {
		throw SyntaxError();
	}
	configure(getWidget(tokens[2]->getString()), tokens);
}

void OSDCommand::configure(OSDWidget& widget, const vector<TclObject*>& tokens)
{
	assert(tokens.size() >= 3);
	if ((tokens.size() - 3) & 1) {
		// odd number of extra arguments
		throw CommandException(
			"Missing value for '" + tokens.back()->getString() + "'.");
	}

	for (unsigned i = 3; i < tokens.size(); i += 2) {
		string name  = tokens[i + 0]->getString();
		string value = tokens[i + 1]->getString();
		widget.setProperty(name, value);
	}
}

string OSDCommand::help(const vector<string>& tokens) const
{
	if (tokens.size() >= 2) {
		if (tokens[1] == "create") {
			return
			  "osd create <type> [<property-name> <property-value>]...\n"
			  "\n"
			  "Creates a new OSD widget of given type. Optionally "
			  "you can set initial values for one or more properties.\n"
			  "This command returns an ID for the newly created widget "
			  "this ID is needed to manipulate the widget with the "
			  "other osd subcommand.";
		} else if (tokens[1] == "destroy") {
			return
			  "osd destroy <widget-id>\n"
			  "\n"
			  "Remove the specified OSD widget.";
		} else if (tokens[1] == "info") {
			return
			  "osd info [<widget-id> [<property-name>]]\n"
			  "\n"
			  "Query various information about the OSD status. "
			  "You can call this command with 0, 1 or 2 arguments.\n"
			  "Without any arguments, this command returns a list "
			  "of all existing widget IDs.\n"
			  "When a widget ID is given as argument, this command "
			  "returns a list of available properties for that widget.\n"
			  "When both widget ID and property name arguments are "
			  "given, this command returns the current value of "
			  "that property.";
		} else if (tokens[1] == "configure") {
			return
			  "osd configure <widget-id> [<property-name> <property-value>]...\n"
			  "\n"
			  "Modify one or more properties on the given widget.";
		} else {
			return "No such subcommand, see 'help osd'.";
		}
	} else {
		return
		  "Low level OSD GUI commands\n"
		  "  osd create <type> [<property-name> <property-value>]...\n"
		  "  osd destroy <widget-id>\n"
		  "  osd info [<widget-id> [<property-name>]]\n"
		  "  osd configure <widget-id> [<property-name> <property-value>]...\n"
		  "Use 'help osd <subcommand>' to see more info on a specific subcommand";
	}
}

void OSDCommand::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
	case 2: {
		set<string> cmds;
		cmds.insert("create");
		cmds.insert("destroy");
		cmds.insert("info");
		cmds.insert("configure");
		completeString(tokens, cmds);
		break;
	}
	case 3: {
		set<string> s;
		if (tokens[1] == "create") {
			s.insert("rectangle");
			s.insert("text");
		} else {
			const OSDGUI::Widgets& widgets = gui.getWidgets();
			for (OSDGUI::Widgets::const_iterator it = widgets.begin();
			     it != widgets.end(); ++it) {
				s.insert((*it)->getName());
			}
		}
		completeString(tokens, s);
		break;
	}
	default: {
		try {
			set<string> properties;
			if (tokens[1] == "create") {
				auto_ptr<OSDWidget> widget = create(tokens[2]);
				widget->getProperties(properties);
			} else if (tokens[1] == "configure") {
				const OSDWidget& widget = getWidget(tokens[2]);
				widget.getProperties(properties);
			}
			completeString(tokens, properties);
		} catch (MSXException& e) {
			// ignore
		}
	}
	}
}

OSDWidget& OSDCommand::getWidget(const string& name) const
{
	const OSDGUI::Widgets& widgets = gui.getWidgets();
	for (OSDGUI::Widgets::const_iterator it = widgets.begin();
	     it != widgets.end(); ++it) {
		if ((*it)->getName() == name) {
			return **it;
		}
	}
	throw CommandException("No widget with name " + name);
}

} // namespace openmsx
