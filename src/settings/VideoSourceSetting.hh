#ifndef VIDEOSOURCESETTING_HH
#define VIDEOSOURCESETTING_HH

#include "Setting.hh"
#include <vector>
#include <utility>

namespace openmsx {

class VideoSourceSetting final : public Setting
{
public:
	explicit VideoSourceSetting(CommandController& commandController);

	string_ref getTypeString() const override;
	void additionalInfo(TclObject& result) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	int registerVideoSource(const std::string& source);
	void unregisterVideoSource(int source);
	int getSource();
	void setSource(int id);

private:
	std::vector<string_ref> getPossibleValues() const;
	void checkSetValue(string_ref value) const;
	bool has(int value) const;
	int has(string_ref value) const;

	using Sources = std::vector<std::pair<std::string, int>>;
	Sources sources; // unordered
};

class VideoSourceActivator
{
public:
	VideoSourceActivator(
		VideoSourceSetting& setting, const std::string& name);
	~VideoSourceActivator();
	int getID() const { return id; }
private:
	VideoSourceSetting& setting;
	int id;
};

} // namespace openmsx

#endif
