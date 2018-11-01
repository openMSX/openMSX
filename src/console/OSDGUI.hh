#ifndef OSDGUI_HH
#define OSDGUI_HH

#include "OSDTopWidget.hh"
#include "Command.hh"
#include <memory>

namespace openmsx {

class Display;
class CommandController;

class OSDGUI
{
public:
	OSDGUI(CommandController& commandController, Display& display);

	Display& getDisplay() const { return display; }
	const OSDTopWidget& getTopWidget() const { return topWidget; }
	      OSDTopWidget& getTopWidget()       { return topWidget; }
	void refresh() const;

	void setOpenGL(bool openGL_) { openGL = openGL_; }
	bool isOpenGL() const { return openGL; }

private:
	Display& display;

	class OSDCommand final : public Command {
	public:
		explicit OSDCommand(CommandController& commandController);
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
			string_view type, const TclObject& name) const;
		void configure(OSDWidget& widget, array_ref<TclObject> tokens,
			       unsigned skip);

		OSDWidget& getWidget(string_view name) const;
	} osdCommand;

	OSDTopWidget topWidget;
	bool openGL;
};

} // namespace openmsx

#endif
