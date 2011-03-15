namespace eval psg_profile {

set_help_text psg_profile \
{Select a PSG sound profile.

Usage:
  psg_profile            Shows the current profile
  psg_profile -list      Shows the possible profiles
  psg_profile <profile>  Select a new profile
}

variable psg_profiles
variable psg_settings

set_tabcompletion_proc psg_profile [namespace code tab_psg_profile]
proc tab_psg_profile { args } {
	variable psg_profiles
	set result [array names psg_profiles]
	lappend result "-list"
}

set psg_settings {::PSG_vibrato_percent ::PSG_vibrato_frequency
                    ::PSG_detune_percent  ::PSG_detune_frequency}
set psg_profiles(normal)         { 0.0  -  0.0  -  }
set psg_profiles(vibrato)        { 1.0 5.0 0.0  -  }
set psg_profiles(detune)         { 0.0  -  0.5 5.0 }
set psg_profiles(detune_vibrato) { 1.0 5.0 0.5 5.0 }

proc equal_psg_profile { values } {
	variable psg_settings
	foreach setting $psg_settings value $values {
		if {$value ne "-"} {
			if {[set $setting] != $value} {
				return false
			}
		}
	}
	return true
}

proc get_psg_profile { } {
	variable psg_settings
	set result [list]
	foreach setting $psg_settings {
		lappend result [set $setting]
	}
	return $result
}

proc set_psg_profile { values } {
	variable psg_settings
	foreach setting $psg_settings value $values {
		if {$value ne "-"} {
			set $setting $value
		}
	}
}

proc psg_profile { {profile ""} } {
	variable psg_settings
	variable psg_profiles
	if [string equal $profile ""] {
		foreach profile [array names psg_profiles] {
			if [equal_psg_profile $psg_profiles($profile)] {
				return $profile
			}
		}
		return "Custom profile: [get_psg_profile]"
	} elseif [string equal $profile "-list"] {
		return [array names psg_profiles]
	} else {
		if [info exists psg_profiles($profile)] {
			set_psg_profile $psg_profiles($profile)
		} else {
			error "No such profile: $profile"
		}
	}
}

namespace export psg_profile

} ;# namespace psg_profile

namespace import psg_profile::*
