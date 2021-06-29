#ifndef OSDTOPWIDGET_HH
#define OSDTOPWIDGET_HH

#include "OSDWidget.hh"
#include "TclObject.hh"
#include "hash_set.hh"
#include <vector>
#include <string>

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
	[[nodiscard]] std::vector<std::string_view> getAllWidgetNames() const;

protected:
	void invalidateLocal() override;
	void paintSDL(OutputSurface& output) override;
	void paintGL (OutputSurface& output) override;

private:
	std::vector<std::string> errors;

	struct NameFromWidget {
		[[nodiscard]] std::string_view operator()(const OSDWidget* w) const {
			return w->getName();
		}
	};
	hash_set<OSDWidget*, NameFromWidget, XXTclHasher> widgetsByName;
};

} // namespace openmsx

#endif
