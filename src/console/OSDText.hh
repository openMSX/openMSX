#ifndef OSDTEXT_HH
#define OSDTEXT_HH

#include "OSDImageBasedWidget.hh"
#include "TTFFont.hh"
#include <memory>

namespace openmsx {

class OSDText final : public OSDImageBasedWidget
{
public:
	OSDText(Display& display, const TclObject& name);

	[[nodiscard]] std::vector<std::string_view> getProperties() const override;
	void setProperty(Interpreter& interp,
	                 std::string_view name, const TclObject& value) override;
	void getProperty(std::string_view name, TclObject& result) const override;
	[[nodiscard]] std::string_view getType() const override;

private:
	void invalidateLocal() override;
	[[nodiscard]] gl::vec2 getSize(const OutputSurface& output) const override;
	[[nodiscard]] uint8_t getFadedAlpha() const override;
	[[nodiscard]] std::unique_ptr<BaseImage> createSDL(OutputSurface& output) override;
	[[nodiscard]] std::unique_ptr<BaseImage> createGL (OutputSurface& output) override;
	template<typename IMAGE> [[nodiscard]] std::unique_ptr<BaseImage> create(
		OutputSurface& output);

	template<typename FindSplitPointFunc, typename CantSplitFunc>
	[[nodiscard]] size_t split(const std::string& line, unsigned maxWidth,
		FindSplitPointFunc findSplitPoint, CantSplitFunc cantSplit,
		bool removeTrailingSpaces) const;
	[[nodiscard]] size_t splitAtChar(const std::string& line, unsigned maxWidth) const;
	[[nodiscard]] size_t splitAtWord(const std::string& line, unsigned maxWidth) const;
	[[nodiscard]] std::string getCharWrappedText(const std::string& txt, unsigned maxWidth) const;
	[[nodiscard]] std::string getWordWrappedText(const std::string& txt, unsigned maxWidth) const;

	[[nodiscard]] gl::vec2 getRenderedSize() const;

private:
	enum WrapMode { NONE, WORD, CHAR };

	std::string text;
	std::string fontfile;
	TTFFont font;
	int size;
	WrapMode wrapMode;
	float wrapw, wraprelw;

	friend struct SplitAtChar;
};

} // namespace openmsx

#endif
