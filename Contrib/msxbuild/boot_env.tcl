# boot_env -- Sets various openMSX settings based from environment variables.
#
# This script should NOT be installed in the ~/.openmsx/share/scripts directory
# (because you don't want it to be active every time you start openMSX). Instead
# you should activate it via the openMSX command line with the option '-script
# boot_env.tcl'.
#
# Typically used in automation tools which run openMSX without human interaction.
# Supported environment variables by this script;
#
# SAVE_SETTINGS_ON_EXIT=false
# Disables automatic settings saving.
#
# RENDERER=none
# Disables video output, to enable video use 'SDL' as argument.
#
# THROTTLE=off
# Disables msx speed emulation.
#
# SPEED=400
# Sets msx speed to 4x of original but only when throttle is on.
#
# JOYPORTA=mouse
# Inserts mouse in joyporta.
#
# JOYPORTB=mouse
# Inserts mouse in joyportb.
#

if {[info exists ::env(SAVE_SETTINGS_ON_EXIT)] && ([string trim $::env(SAVE_SETTINGS_ON_EXIT)] != "")} {
	if {[catch {set save_settings_on_exit [string trim $::env(SAVE_SETTINGS_ON_EXIT)]} err_msg]} {
		puts stderr "error: env.SAVE_SETTINGS_ON_EXIT value $err_msg"
		exit 1
	}
}

if {[info exists ::env(RENDERER)] && ([string trim $::env(RENDERER)] != "")} {
	if {[catch {set renderer [string trim $::env(RENDERER)]} err_msg]} {
		puts stderr "error: env.RENDERER value $err_msg"
		exit 1
	}
}

if {[info exists ::env(THROTTLE)] && ([string trim $::env(THROTTLE)] != "")} {
	if {[catch {set throttle [string trim $::env(THROTTLE)]} err_msg]} {
		puts stderr "error: env.THROTTLE value $err_msg"
		exit 1
	}
}

if {[info exists ::env(SPEED)] && ([string trim $::env(SPEED)] != "")} {
	if {[catch {set speed [string trim $::env(SPEED)]} err_msg]} {
		puts stderr "error: env.SPEED value $err_msg"
		exit 1
	}
}

if {[info exists ::env(JOYPORTA)] && ([string trim $::env(JOYPORTA)] != "")} {
	if {[catch {plug joyporta [string trim $::env(JOYPORTA)]} err_msg]} {
		puts stderr "error: env.JOYPORTA value $err_msg"
		exit 1
	}
}

if {[info exists ::env(JOYPORTB)] && ([string trim $::env(JOYPORTB)] != "")} {
	if {[catch {plug joyportb [string trim $::env(JOYPORTB)]} err_msg]} {
		puts stderr "error: env.JOYPORTB value $err_msg"
		exit 1
	}
}
