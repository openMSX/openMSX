#include <SDL/SDL.h>
#include "GrabInput.hh"

GrabInputSetting::GrabInputSetting()
	: BooleanSetting(
		"grabinput",
		"This setting controls if openmsx takes over mouse and keyboard input",
		false)
{
}

bool GrabInputSetting::checkUpdate(bool newValue)
{
	if (newValue) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
	}
	return true;
}
