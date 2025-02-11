#ifndef IMGUI_WATCH_EXPR_HH
#define IMGUI_WATCH_EXPR_HH

#include "ImGuiPart.hh"

#include "TclObject.hh"

#include <expected>
#include <vector>

namespace openmsx {

class SymbolManager;

class ImGuiWatchExpr final : public ImGuiPart
{
public:
	explicit ImGuiWatchExpr(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "watch expr"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;
	void refreshSymbols();

private:
	void drawRow(int row);
	void checkSort();

public:
	bool show = false;

private:
	SymbolManager& symbolManager;

	struct WatchExpr {
		// added constructors: workaround clang-14 bug(?)
		WatchExpr() = default;
		WatchExpr(std::string d, std::string e, TclObject f)
			: description(std::move(d)), exprStr(std::move(e)), format(std::move(f)) {}
		std::string description;
		std::string exprStr;
		std::optional<TclObject> expression; // cache, generate from 'expression'
		TclObject format;
	};
	std::vector<WatchExpr> watches;

	[[nodiscard]] std::expected<TclObject, std::string> evalExpr(WatchExpr& watch, Interpreter& interp) const;

	int selectedRow = -1;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiWatchExpr::show},
		// manually handle 'watches'
	};
};

} // namespace openmsx

#endif
