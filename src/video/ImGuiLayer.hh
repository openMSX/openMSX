#ifndef IMGUILAYER_HH
#define IMGUILAYER_HH

#include "Layer.hh"
#include "TclObject.hh"
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pfd {
	class open_file;
}
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
	void selectFileCommand(const std::string& title, TclObject command);

private:
	Reactor& reactor;
	Interpreter& interp;
	std::map<std::string, std::unique_ptr<DebuggableEditor>> debuggables;
	std::unique_ptr<pfd::open_file> openFileDialog;
	std::function<void(const std::vector<std::string>&)> openFileCallback;
	bool wantOpenModal = false; //WIP
	bool showDemoWindow = false;
	bool first = true;
};

} // namespace openmsx

#endif
