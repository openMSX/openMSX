include build/node-start.mk

DIST:= \
	AndroidAppSettings.cfg.template.* \
	AndroidBuild.sh generate_AndroidAppSettings.sh launch_anddev_build.sh

include build/node-end.mk
