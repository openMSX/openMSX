#ifndef OSDTEXT_HH
#define OSDTEXT_HH

#include "OSDImageBasedWidget.hh"
#include "TTFFont.hh"

#include "stl.hh"

#include <array>
#include <memory>

namespace openmsx {

class OSDText final : public OSDImageBasedWidget
{
private:
	static constexpr auto textProperties = [] {
		using namespace std::literals;
		return concatArray(
			imageBasedProperties,
			std::array{
				"-text"sv, "-font"sv, "-size"sv,
				"-wrap"sv, "-wrapw"sv, "-wraprelw"sv,
			});
	}();

public:
	OSDText(Display& display, const TclObject& name);

	[[nodiscard]] std::span<const std::string_view> getProperties() const override {
		return textProperties;
	}
	void setProperty(Interpreter& interp,
	                 std::string_view name, const TclObject& value) override;
	void getProperty(std::string_view name, TclObject& result) const override;
	[[nodiscard]] std::string_view getType() const override;

private:
	void invalidateLocal() override;
	[[nodiscard]] gl::vec2 getSize(const OutputSurface& output) const override;
	[[nodiscard]] uint8_t getFadedAlpha() const override;
	[[nodiscard]] std::unique_ptr<GLImage> create(OutputSurface& output) override;

	template<typename FindSplitPointFunc, typename CantSplitFunc>
	[[nodiscard]] size_t split(const std::string& line, unsigned maxWidth,
		FindSplitPointFunc findSplitPoint, CantSplitFunc cantSplit,
		bool removeTrailingSpaces) const;
	[[nodiscard]] size_t splitAtChar(const std::string& line, unsigned maxWidth) const;
	[[nodiscard]] size_t splitAtWord(const std::string& line, unsigned maxWidth) const;
	[[nodiscard]] std::string getCharWrappedText(const std::string& txt, unsigned maxWidth) const;
	[[nodiscard]] std::string getWordWrappedText(const std::string& txt, unsigned maxWidth) const;

private:
	enum WrapMode { NONE, WORD, CHAR };

	std::string text;
	std::string fontFile;
	TTFFont font;
	int size = 12;
	WrapMode wrapMode = NONE;
	float wrapw = 0.0f, wraprelw = 1.0f;

	friend struct SplitAtChar;
};

} // namespace openmsx

#endif
