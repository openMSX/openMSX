#ifndef VIDEOSOURCESETTING_HH
#define VIDEOSOURCESETTING_HH

#include "Setting.hh"
#include <vector>

namespace openmsx {

class VideoSourceSetting final : public Setting
{
public:
	explicit VideoSourceSetting(CommandController& commandController);

	[[nodiscard]] std::string_view getTypeString() const override;
	void additionalInfo(TclObject& result) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	[[nodiscard]] int registerVideoSource(const std::string& source);
	void unregisterVideoSource(int source);
	[[nodiscard]] int getSource() noexcept;
	void setSource(int id);
	[[nodiscard]] std::vector<std::string_view> getPossibleValues() const;

private:
	void checkSetValue(std::string_view value) const;
	[[nodiscard]] bool has(int value) const;
	[[nodiscard]] int has(std::string_view value) const;

private:
	struct Source {
		Source(std::string n, int i)
			: name(std::move(n)), id(i) {} // clang-15 workaround

		std::string name;
		int id;
	};
	std::vector<Source> sources; // unordered
};

class VideoSourceActivator
{
public:
	VideoSourceActivator(VideoSourceSetting& setting, const std::string& name);
	VideoSourceActivator(const VideoSourceActivator&) = delete;
	VideoSourceActivator(VideoSourceActivator&&) = delete;
	VideoSourceActivator& operator=(const VideoSourceActivator&) = delete;
	VideoSourceActivator& operator=(VideoSourceActivator&&) = delete;
	~VideoSourceActivator();

	[[nodiscard]] int getID() const { return id; }

private:
	VideoSourceSetting& setting;
	int id;
};

} // namespace openmsx

#endif
