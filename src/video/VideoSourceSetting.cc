#include "VideoSourceSetting.hh"
#include "CommandException.hh"

namespace openmsx {

VideoSourceSettingPolicy::VideoSourceSettingPolicy(const Map& map)
	: EnumSettingPolicy<int>(map)
{
}

bool VideoSourceSettingPolicy::has(int value) const
{
	for (auto& p : baseMap) {
		if (p.second == value) return true;
	}
	return false;
}

int VideoSourceSettingPolicy::has(const std::string& value) const
{
	auto it = baseMap.find(value);
	return (it != baseMap.end()) ? it->second : 0;
}

void VideoSourceSettingPolicy::checkSetValue(int& value) const
{
	// Special case: in case there are no videosources registered (yet),
	// the only allowed value is "none". In case there is at least one
	// registered source, this special value "none" should be hidden.
	if (((value == 0) && (baseMap.size() >  1)) ||
	    ((value != 0) && (baseMap.size() == 1)) ||
	    (!has(value))) {
		throw CommandException("video source not available");
	}
}

int VideoSourceSettingPolicy::checkGetValue(int value) const
{
	// For the prefered value (initial value or new value in case the
	// old value became invalid) the order of values is based on fixed
	// names. Good enough for now.
	if ((value != 0) && has(value)) {
		// already a valid value
		return value;
	} else if (int id = has("Video9000")) {
		// prefer video9000 over v99x8
		return id;
	} else if (int id = has("MSX")) {
		// V99x9 over V9990
		return id;
	} else if (int id = has("GFX9000")) {
		// and V9990 over laserdisc
		return id;
	} else if (int id = has("Laserdisc")) {
		return id;
	} else {
		// this handles the "none" case, but also stuff like
		// multiple V99x8/V9990 chips
		int newest = 0;
		for (auto& p : baseMap) {
			newest = std::max(newest, p.second);
		}
		return newest;
	}
}

static VideoSourceSetting::Map getVideoSourceMap()
{
	VideoSourceSetting::Map result;
	result["none"] = 0;
	return result;
}

VideoSourceSetting::VideoSourceSetting(CommandController& commandController)
	: SettingImpl<VideoSourceSettingPolicy>(commandController,
		"videosource", "selects the video source to display on the screen",
		0, Setting::DONT_SAVE, getVideoSourceMap())
{
}

int VideoSourceSetting::registerVideoSource(const std::string& source)
{
	static int counter = 0; // id's are globally unique

	assert(!has(source));
	baseMap[source] = ++counter;

	// First announce extended set of allowed values before announcing a
	// (possibly) different value.
	notifyPropertyChange();
	changeValue(getValue());

	return counter;
}

void VideoSourceSetting::unregisterVideoSource(int source)
{
	auto it = find_if(baseMap.begin(), baseMap.end(),
		[&](Map::value_type& p) { return p.second == source; });
	assert(it != baseMap.end());
	baseMap.erase(it);

	// First notify the (possibly) changed value before announcing the
	// shrinked set of values.
	changeValue(getValue());
	notifyPropertyChange();
}


VideoSourceActivator::VideoSourceActivator(
	VideoSourceSetting& setting_, const std::string& name)
	: setting(setting_)
{
	id = setting.registerVideoSource(name);
}

VideoSourceActivator::~VideoSourceActivator()
{
	setting.unregisterVideoSource(id);
}

} // namespace openmsx
