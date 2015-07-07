#ifndef OSDTOPWIDGET_HH
#define OSDTOPWIDGET_HH

#include "OSDWidget.hh"
#include "TclObject.hh"
#include "hash_map.hh"
#include "xxhash.hh"
#include <vector>
#include <string>

namespace openmsx {

class OSDGUI;

struct XXTclHasher {
	uint32_t operator()(string_ref str) const {
		return xxhash(str);
	}
	uint32_t operator()(const TclObject& obj) const {
		return xxhash(obj.getString());
	}
};

class OSDTopWidget final : public OSDWidget
{
public:
	OSDTopWidget(OSDGUI& gui);
	string_ref getType() const override;
	gl::vec2 getSize(const OutputRectangle& output) const override;

	void queueError(std::string message);
	void showAllErrors();

	OSDWidget* findByName(string_ref name);
	const OSDWidget* findByName(string_ref name) const;
	void addName(const TclObject& name, OSDWidget& widget);
	void removeName(string_ref name);
	std::vector<string_ref> getAllWidgetNames() const;

protected:
	void invalidateLocal() override;
	void paintSDL(OutputSurface& output) override;
	void paintGL (OutputSurface& output) override;

private:
	OSDGUI& gui;
	std::vector<std::string> errors;
	hash_map<TclObject, OSDWidget*, XXTclHasher> widgetsByName;
};

} // namespace openmsx

#endif
