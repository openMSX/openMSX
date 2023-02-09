#ifndef IMGUILAYER_HH
#define IMGUILAYER_HH

#include "Layer.hh"
#include "TclObject.hh"
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace openmsx {

class DebuggableEditor;
class Interpreter;
class MSXMotherBoard;
class Reactor;

class ImGuiLayer final : public Layer
{
public:
	ImGuiLayer(Reactor& reactor);
	~ImGuiLayer() override;

private:
	// Layer
	void paint(OutputSurface& output) override;

	void mediaMenu(MSXMotherBoard* motherBoard);
	void connectorsMenu(MSXMotherBoard* motherBoard);
	void debuggableMenu(MSXMotherBoard* motherBoard);

	std::optional<TclObject> execute(TclObject command);

private:
	Reactor& reactor;
	Interpreter& interp;
	std::map<std::string, std::unique_ptr<DebuggableEditor>> debuggables;
	unsigned modalID = 0; //WIP
	bool show_demo_window = false;
	bool first = true;
};

} // namespace openmsx

#endif
