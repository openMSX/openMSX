#include "JoystickManager.hh"

#include "IntegerSetting.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "strCat.hh"

namespace openmsx {

JoystickManager::JoystickManager(CommandController& commandController_)
	: commandController(commandController_)
{
	// Note: we don't explicitly enumerate all joysticks which are already
	// present at startup. Instead we rely on SDL_JOYDEVICEADDED events:
	// these also get send for the initial joysticks.

	// joysticks generate events
	SDL_JoystickEventState(SDL_ENABLE); // is this needed?
}

JoystickManager::~JoystickManager()
{
	for (auto& info : infos) {
		if (info.joystick) SDL_JoystickClose(info.joystick);
	}
}

size_t JoystickManager::getFreeSlot()
{
	if (auto it = ranges::find(infos, nullptr, &Info::joystick); it != infos.end()) {
		return std::distance(infos.begin(), it);
	}
	auto idx = infos.size();
	infos.emplace_back();
	infos[idx].deadZoneSetting = std::make_unique<IntegerSetting>(
		commandController,
		tmpStrCat("joystick", idx + 1, "_deadzone"),
		"size (as a percentage) of the dead center zone",
		25, 0, 100);
	return idx;
}

void JoystickManager::add(int deviceIndex)
{
	auto idx = getFreeSlot();
	auto& info = infos[idx];
	info.joystick = SDL_JoystickOpen(deviceIndex);
	info.instanceId = info.joystick ? SDL_JoystickInstanceID(info.joystick) : -1;
}

void JoystickManager::remove(int instanceId)
{
	auto it = ranges::find(infos, instanceId, &Info::instanceId);
	if (it == infos.end()) return; // this shouldn't happen

	SDL_JoystickClose(it->joystick);
	it->joystick = nullptr;
	it->instanceId = -1;
}

std::vector<JoystickId> JoystickManager::getConnectedJoysticks() const
{
	// TODO c++20 view::filter | view::transform
	std::vector<JoystickId> result;
	for (auto i : xrange(infos.size())) {
		if (!infos[i].joystick) continue;
		result.emplace_back(unsigned(i));
	}
	return result;
}

IntegerSetting* JoystickManager::getJoyDeadZoneSetting(JoystickId joyId) const
{
	unsigned id = joyId.raw();
	return (id < infos.size()) ? infos[id].deadZoneSetting.get() : nullptr;
}

std::string JoystickManager::getDisplayName(JoystickId joyId) const
{
	unsigned id = joyId.raw();
	auto* joystick = (id < infos.size()) ? infos[id].joystick : nullptr;
	return joystick ? SDL_JoystickName(joystick) : "[Not plugged in]";
}

std::optional<unsigned> JoystickManager::getNumButtons(JoystickId joyId) const
{
	unsigned id = joyId.raw();
	auto* joystick = (id < infos.size()) ? infos[id].joystick : nullptr;
	if (!joystick) return {};
	return SDL_JoystickNumButtons(joystick);
}

std::optional<JoystickId> JoystickManager::translateSdlInstanceId(SDL_Event& evt) const
{
	int instanceId = evt.jbutton.which; // any SDL joystick event
	auto it = ranges::find(infos, instanceId, &Info::instanceId);
	if (it == infos.end()) return {}; // should not happen
	evt.jbutton.which = narrow<int>(std::distance(infos.begin(), it));
	return JoystickId(evt.jbutton.which);
}

} // namespace openmsx
