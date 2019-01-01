# boot_hdd -- Create import/export disk images to local folder.
#
# This script should NOT be installed in the ~/.openmsx/share/scripts directory
# (because you don't want it to be active every time you start openMSX). Instead
# you should activate it via the openMSX command line with the option '-script
# boot_hdd.tcl'.
#
# Typically used in automation tools which run openMSX without human interaction.
# This script is basicly is a bit more generic and safe way to run the following code;
#
# diskmanipulator create disk.img 32m 32m 32m 32m
# hda disk.img
# diskmanipulator import hda1 ./disk-in/
# after quit {diskmanipulator export hda1 ./disk-out/}
#
#
# Supported environment variables by this script;
#
# BOOT_HDD_SIZE=30m
# Sets the size of the created partitions, defaults to 15m.
#
# BOOT_HDD_IMAGE=bin/myapp/dsk.img
# Defaults to ./hdd.dsk
#
# BOOT_HDD_PATH=bin/myapp/dsk
# provides the default values for BOOT_HDD_PATH_IMPORT and BOOT_HDD_PATH_EXPORT
#
# BOOT_HDD_PATH_IMPORT=bin/myapp/dsk
# When set enables the import of all files into the first disk partition.
#
# BOOT_HDD_PATH_EXPORT=bin/myapp/dsk-result
# When set enables the export of all files back to the filesystem
#
# BOOT_HDD_EXPORT_PARTITION=2
# When set override the export from 'first' to 'given' partition number.
#
# BOOT_HDD_EXPORT_DIR=myout
# By default export does chdir to root of msx partition override to custom export directory.
#
# BOOT_HDD_PARTITIONS=2
# The number of partitions created in the disk image, defaults to 1.
#

# per default create msxdos1 compatible partition size.
set boot_hdd_size 15m
set boot_hdd_image hdd.dsk
set boot_hdd_path_import 0
set boot_hdd_path_export 0
set boot_hdd_export_partition 0
set boot_hdd_export_dir \\
set boot_hdd_partitions 1

# Parse env settings
if {[info exists ::env(BOOT_HDD_SIZE)] && ([string trim $::env(BOOT_HDD_SIZE)] != "")} {
	set boot_hdd_size [string trim $::env(BOOT_HDD_SIZE)]
}
if {[info exists ::env(BOOT_HDD_IMAGE)] && ([string trim $::env(BOOT_HDD_IMAGE)] != "")} {
	set boot_hdd_image [string trim $::env(BOOT_HDD_IMAGE)]
}
if {[info exists ::env(BOOT_HDD_PATH)] && ([string trim $::env(BOOT_HDD_PATH)] != "")} {
	set boot_hdd_path_import [string trim $::env(BOOT_HDD_PATH)]
	set boot_hdd_path_export [string trim $::env(BOOT_HDD_PATH)]
}
if {[info exists ::env(BOOT_HDD_PATH_IMPORT)] && ([string trim $::env(BOOT_HDD_PATH_IMPORT)] != "")} {
	set boot_hdd_path_import [string trim $::env(BOOT_HDD_PATH_IMPORT)]
}
if {[info exists ::env(BOOT_HDD_PATH_EXPORT)] && ([string trim $::env(BOOT_HDD_PATH_EXPORT)] != "")} {
	set boot_hdd_path_export [string trim $::env(BOOT_HDD_PATH_EXPORT)]
}
if {[info exists ::env(BOOT_HDD_EXPORT_PARTITION)] && ([string trim $::env(BOOT_HDD_EXPORT_PARTITION)] != "")} {
	set boot_hdd_export_partition [string trim $::env(BOOT_HDD_EXPORT_PARTITION)]
}
if {[info exists ::env(BOOT_HDD_EXPORT_DIR)] && ([string trim $::env(BOOT_HDD_EXPORT_DIR)] != "")} {
	set boot_hdd_export_dir [string trim $::env(BOOT_HDD_EXPORT_DIR)]
}
if {[info exists ::env(BOOT_HDD_PARTITIONS)] && ([string trim $::env(BOOT_HDD_PARTITIONS)] != "")} {
	set boot_hdd_partitions [string trim $::env(BOOT_HDD_PARTITIONS)]
	if {$boot_hdd_partitions == 0 || $boot_hdd_partitions > 4} {
		puts stderr "error: Invalid env.BOOT_HDD_PARTITIONS value 1-4 allowed: $boot_hdd_partitions"
		exit 1
	}
}

if {$boot_hdd_path_import != 0} {
	set boot_hdd_disk_partition "hda"
	if {$boot_hdd_partitions == 1} {
		if {[catch {diskmanipulator create $boot_hdd_image $boot_hdd_size} err_msg]} {
			puts stderr "error: create1 $err_msg"
			exit 1
		}
	}
	if {$boot_hdd_partitions > 1} {
		set boot_hdd_disk_partition "hda1"
		if {$boot_hdd_partitions == 2} {
			if {[catch {diskmanipulator create $boot_hdd_image $boot_hdd_size $boot_hdd_size} err_msg]} {
				puts stderr "error: create2 $err_msg"
				exit 1
			}
		}
		if {$boot_hdd_partitions == 3} {
			if {[catch {diskmanipulator create $boot_hdd_image $boot_hdd_size $boot_hdd_size $boot_hdd_size} err_msg]} {
				puts stderr "error: create3 $err_msg"
				exit 1
			}
		}
		if {$boot_hdd_partitions == 4} {
			if {[catch {diskmanipulator create $boot_hdd_image $boot_hdd_size $boot_hdd_size $boot_hdd_size $boot_hdd_size} err_msg]} {
				puts stderr "error: create4 $err_msg"
				exit 1
			}
		}
	}
	if {[catch {hda $boot_hdd_image} err_msg]} {
		puts stderr "error: hda $err_msg"
		exit 1
	}
	if {[catch {diskmanipulator import $boot_hdd_disk_partition $boot_hdd_path_import} err_msg]} {
		puts stderr "error: import $err_msg"
		exit 1
	}
	if {$boot_hdd_path_export != 0} {
		if {$boot_hdd_export_partition != 0} {
			set boot_hdd_disk_partition "hda$boot_hdd_export_partition"
		}
		after quit {
			if {[catch {diskmanipulator chdir $boot_hdd_disk_partition $boot_hdd_export_dir} err_msg]} {
				puts stderr "error: chdir $err_msg"
				exit 1
			}
			if {[catch {diskmanipulator export $boot_hdd_disk_partition $boot_hdd_path_export} err_msg]} {
				puts stderr "error: export $err_msg"
				exit 1
			}
		}
	}
}
