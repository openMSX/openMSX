#ifndef VIDEOSOURCESETTING_HH
#define VIDEOSOURCESETTING_HH

#include "EnumSetting.hh"

namespace openmsx {

class VideoSourceSettingPolicy : public EnumSettingPolicy<int>
{
protected:
	VideoSourceSettingPolicy(const Map& map);
	virtual void checkSetValue(int& value) const;
	int checkGetValue(int value) const;
	bool has(int value) const;
	int has(const std::string& value) const;
};


class VideoSourceSetting : public SettingImpl<VideoSourceSettingPolicy>
{
public:
	explicit VideoSourceSetting(CommandController& commandController);
	int registerVideoSource(const std::string& source);
	void unregisterVideoSource(int source);

	int getSource() const { return this->getValue(); }
	void setSource(int s) { this->changeValue(s); }
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
