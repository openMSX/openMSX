# Cycle through the possible values of an enumsettings
#
# Usage:
#   cycle <enumsetting> [<step>]
#
# Example:
#   cycle scaler

proc cycle {setting {step 1}} {
	if [catch {
		set values [openmsx_info $setting]
		set cur [lsearch -exact $values [set ::$setting]]
	} ] {
		error "Not an enum setting: $setting"
	}
	set new [expr ($cur + $step) % [llength $values]]
	set ::$setting [lindex $values $new]
}
