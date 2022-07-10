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

	[[nodiscard]] Display& getDisplay() const { return display; }
	[[nodiscard]] const OSDTopWidget& getTopWidget() const { return topWidget; }
	[[nodiscard]]       OSDTopWidget& getTopWidget()       { return topWidget; }
	void refresh() const;

	void setOpenGL(bool openGL_) { openGL = openGL_; }
	[[nodiscard]] bool isOpenGL() const { return openGL; }

private:
	Display& display;

	class OSDCommand final : public Command {
	public:
		explicit OSDCommand(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
		void create   (std::span<const TclObject> tokens, TclObject& result);
		void destroy  (std::span<const TclObject> tokens, TclObject& result);
		void info     (std::span<const TclObject> tokens, TclObject& result);
		void exists   (std::span<const TclObject> tokens, TclObject& result);
		void configure(std::span<const TclObject> tokens, TclObject& result);
		[[nodiscard]] std::unique_ptr<OSDWidget> create(
			std::string_view type, const TclObject& name) const;
		void configure(OSDWidget& widget, std::span<const TclObject> tokens);

		[[nodiscard]] OSDWidget& getWidget(std::string_view name) const;
	} osdCommand;

	OSDTopWidget topWidget;
	bool openGL;
};

} // namespace openmsx

#endif
