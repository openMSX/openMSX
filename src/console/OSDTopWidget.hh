#ifndef OSDTOPWIDGET_HH
#define OSDTOPWIDGET_HH

#include "OSDWidget.hh"

#include "TclObject.hh"
#include "hash_set.hh"

#include <ranges>
#include <string>
#include <vector>

namespace openmsx {

class OSDTopWidget final : public OSDWidget
{
public:
	explicit OSDTopWidget(Display& display);
	[[nodiscard]] std::string_view getType() const override;
	[[nodiscard]] gl::vec2 getSize(const OutputSurface& output) const override;
	[[nodiscard]] bool isVisible() const override;
	[[nodiscard]] bool isRecursiveFading() const override;

	void queueError(std::string message);
	void showAllErrors();

	[[nodiscard]]       OSDWidget* findByName(std::string_view name);
	[[nodiscard]] const OSDWidget* findByName(std::string_view name) const;
	void addName(OSDWidget& widget);
	void removeName(OSDWidget& widget);
	[[nodiscard]] auto getAllWidgetNames() const {
		return std::views::transform(widgetsByName,
			[](auto* p) -> std::string_view { return p->getName(); });
	}


protected:
	void invalidateLocal() override;
	void paint(OutputSurface& output) override;

private:
	std::vector<std::string> errors;

	struct NameFromWidget {
		[[nodiscard]] static std::string_view operator()(const OSDWidget* w) {
			return w->getName();
		}
	};
	hash_set<OSDWidget*, NameFromWidget, XXTclHasher> widgetsByName;
};

} // namespace openmsx

#endif
