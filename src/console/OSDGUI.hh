#ifndef OSDGUI_HH
#define OSDGUI_HH

#include "Command.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class OSDTopWidget;
class OSDWidget;
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

	class OSDCommand final : public Command {
	public:
		OSDCommand(OSDGUI& gui, CommandController& commandController);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
		void create   (array_ref<TclObject> tokens, TclObject& result);
		void destroy  (array_ref<TclObject> tokens, TclObject& result);
		void info     (array_ref<TclObject> tokens, TclObject& result);
		void exists   (array_ref<TclObject> tokens, TclObject& result);
		void configure(array_ref<TclObject> tokens, TclObject& result);
		std::unique_ptr<OSDWidget> create(
			string_ref type, const std::string& name) const;
		void configure(OSDWidget& widget, array_ref<TclObject> tokens,
			       unsigned skip);

		OSDWidget& getWidget(string_ref name) const;

		OSDGUI& gui;
	} osdCommand;

	const std::unique_ptr<OSDTopWidget> topWidget;
	bool openGL;
};

} // namespace openmsx

#endif
