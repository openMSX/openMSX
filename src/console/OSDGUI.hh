// $Id$

#ifndef OSDGUI_HH
#define OSDGUI_HH

#include "noncopyable.hh"
#include <vector>
#include <memory>

namespace openmsx {

class OSDWidget;
class OSDCommand;
class Display;
class CommandController;

class OSDGUI : private noncopyable
{
public:
	OSDGUI(CommandController& commandController, Display& display);
	~OSDGUI();

	typedef std::vector<OSDWidget*> Widgets;
	const Widgets& getWidgets() const;
	void addWidget(std::auto_ptr<OSDWidget> widget);
	void deleteWidget(OSDWidget& widget);
	void resort();

	Display& getDisplay() const;

private:
	Display& display;
	const std::auto_ptr<OSDCommand> osdCommand;

	Widgets widgets;
};

} // namespace openmsx

#endif
