#ifndef OSDGUI_HH
#define OSDGUI_HH

#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class OSDTopWidget;
class OSDCommand;
class Display;
class CommandController;

class OSDGUI : private noncopyable
{
public:
	OSDGUI(CommandController& commandController, Display& display);
	~OSDGUI();

	Display& getDisplay() const { return display; }
	OSDTopWidget& getTopWidget() const { return *topWidget; }
	void refresh() const;

	void setOpenGL(bool openGL_) { openGL = openGL_; }
	bool isOpenGL() const { return openGL; };

private:
	Display& display;
	const std::unique_ptr<OSDCommand> osdCommand;
	const std::unique_ptr<OSDTopWidget> topWidget;
	bool openGL;
};

} // namespace openmsx

#endif
