#include "VideoSourceSetting.hh"

#include "CommandException.hh"
#include "Completer.hh"

#include "StringOp.hh"
#include "stl.hh"

#include <algorithm>

namespace openmsx {

VideoSourceSetting::VideoSourceSetting(CommandController& commandController_)
	: Setting(commandController_, "videosource",
	          "selects the video source to display on the screen",
	          TclObject("none"), Save::NO)
{
	sources = { {.name = "none", .id = 0} };

	setChecker([this](const TclObject& newValue) {
		checkSetValue(newValue.getString()); // may throw
	});
	init();
}

void VideoSourceSetting::checkSetValue(std::string_view newValue) const
{
	// Special case: in case there are no video sources registered (yet),
	// the only allowed value is "none". In case there is at least one
	// registered source, this special value "none" should be hidden.
	if (((newValue == "none") && (sources.size() >  1)) ||
	    ((newValue != "none") && !has(newValue))) {
		throw CommandException("video source not available");
	}
}

int VideoSourceSetting::getSource() noexcept
{
	// Always try to find a better value than "none".
	if (std::string_view str = getValue().getString();
	    str != "none") {
		// If current value is allowed, then keep it.
		if (int id = has(str)) {
			return id;
		}
	}
	// Search the best value from the current set of allowed values.
	int id = 0;
	if (!id) { id = has("Video9000"); } // in
	if (!id) { id = has("MSX");       } //  order
	if (!id) { id = has("GFX9000");   } //   of
	if (!id) { id = has("Laserdisc"); } //    preference
	if (!id) {
		// This handles the "none" case, but also stuff like
		// multiple V99x8/V9990 chips. Prefer the source with
		// highest id (=newest).
		id = max_value(sources, &Source::id);
	}
	setSource(id); // store new value
	return id;
}

void VideoSourceSetting::setSource(int id)
{
	auto it = find_unguarded(sources, id, &Source::id);
	setValue(TclObject(it->name));
}

std::string_view VideoSourceSetting::getTypeString() const
{
	return "enumeration";
}

std::vector<std::string_view> VideoSourceSetting::getPossibleValues() const
{
	std::vector<std::string_view> result;
	if (sources.size() == 1) {
		assert(sources.front().name == "none");
		result.emplace_back("none");
	} else {
		for (const auto& [name, val] : sources) {
			if (val != 0) {
				result.emplace_back(name);
			}
		}
	}
	return result;
}

void VideoSourceSetting::additionalInfo(TclObject& result) const
{
	TclObject valueList;
	valueList.addListElements(getPossibleValues());
	result.addListElement(valueList);
}

void VideoSourceSetting::tabCompletion(std::vector<std::string>& tokens) const
{
	Completer::completeString(tokens, getPossibleValues(),
	                          false); // case insensitive
}

int VideoSourceSetting::registerVideoSource(const std::string& source)
{
	static int counter = 0; // id's are globally unique

	assert(!has(source));
	sources.emplace_back(source, ++counter);

	// First announce extended set of allowed values before announcing a
	// (possibly) different value.
	notifyPropertyChange();
	setSource(getSource()); // via source to (possibly) adjust value

	return counter;
}

void VideoSourceSetting::unregisterVideoSource(int source)
{
	move_pop_back(sources, rfind_unguarded(sources, source, &Source::id));

	// First notify the (possibly) changed value before announcing the
	// shrunken set of values.
	setSource(getSource()); // via source to (possibly) adjust value
	notifyPropertyChange();
}

bool VideoSourceSetting::has(int val) const
{
	return contains(sources, val, &Source::id);
}

int VideoSourceSetting::has(std::string_view val) const
{
	auto it = std::ranges::find_if(sources, [&](auto& p) {
		StringOp::casecmp cmp;
		return cmp(p.name, val);
	});
	return (it != end(sources)) ? it->id : 0;
}


VideoSourceActivator::VideoSourceActivator(
	VideoSourceSetting& setting_, const std::string& name)
	: setting(setting_)
	, id(setting.registerVideoSource(name))
{
}

VideoSourceActivator::~VideoSourceActivator()
{
	setting.unregisterVideoSource(id);
}

} // namespace openmsx
