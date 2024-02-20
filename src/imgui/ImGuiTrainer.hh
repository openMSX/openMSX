#ifndef IMGUI_TRAINER_HH
#define IMGUI_TRAINER_HH

#include "ImGuiPart.hh"

#include "TclObject.hh"

#include <optional>

namespace openmsx {

class ImGuiTrainer final : public ImGuiPart
{
public:
	explicit ImGuiTrainer(ImGuiManager& manager_)
		: ImGuiPart(manager_) {}

	[[nodiscard]] zstring_view iniName() const override { return "trainer"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	std::optional<TclObject> trainers;
	std::vector<std::string> gameNames; // calculated from 'trainers'
	std::string filterString;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiTrainer::show},
	};
};

} // namespace openmsx

#endif
