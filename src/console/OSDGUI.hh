// $Id$

#ifndef OSDGUI_HH
#define OSDGUI_HH

#include "noncopyable.hh"
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

	Display& getDisplay() const;
	OSDWidget& getTopWidget() const;
	void refresh() const;

private:
	Display& display;
	const std::auto_ptr<OSDCommand> osdCommand;
	const std::auto_ptr<OSDWidget> topWidget;
};

} // namespace openmsx

#endif
