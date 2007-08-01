set_help_text psg_profile \
{Select a PSG sound profile.

Usage:
  psg_profile            Shows the current profile
  psg_profile -list      Shows the possible profiles
  psg_profile <profile>  Select a new profile
}

set_tabcompletion_proc psg_profile tab_psg_profile
proc tab_psg_profile { args } {
	set result [array names ::__psg_profile]
	lappend result "-list"
}

set __psg_settings {::PSG_vibrato_percent ::PSG_vibrato_frequency
                    ::PSG_detune_percent  ::PSG_detune_frequency}
set __psg_profile(normal)         { 0.0  -  0.0  -  }
set __psg_profile(vibrato)        { 1.0 5.0 0.0  -  }
set __psg_profile(detune)         { 0.0  -  1.0 5.0 }
set __psg_profile(detune_vibrato) { 1.0 5.0 1.0 5.0 }

proc __equal_psg_profile { values } {
	foreach setting $::__psg_settings value $values {
		if {$value != "-"} {
			if {[set $setting] != $value} {
				return false
			}
		}
	}
	return true
}

proc __get_psg_profile { } {
	set result [list]
	foreach setting $::__psg_settings {
		lappend result [set $setting]
	}
	return $result
}

proc __set_psg_profile { values } {
	foreach setting $::__psg_settings value $values {
		if {$value != "-"} {
			set $setting $value
		}
	}
}

proc psg_profile { {profile ""} } {
	if [string equal $profile ""] {
		foreach profile [array names ::__psg_profile] {
			if [__equal_psg_profile $::__psg_profile($profile)] {
				return $profile
			}
		}
		return "Custom profile: [__get_psg_profile]"
	} elseif [string equal $profile "-list"] {
		return [array names ::__psg_profile]
	} else {
		if [info exists ::__psg_profile($profile)] {
			__set_psg_profile $::__psg_profile($profile)
		} else {
			error "No such profile: $profile"
		}
	}
}
