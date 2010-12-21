# $Id$

include build/node-start.mk

SRC_HDR:= \
	ScalerFactory \
	Scaler1

ifneq ($(MAX_SCALE_FACTOR), 1)
SRC_HDR += \
	Scanline \
	Scaler2 Scaler3 \
	Simple2xScaler Simple3xScaler \
	SaI2xScaler SaI3xScaler \
	Scale2xScaler Scale3xScaler \
	HQ2xScaler HQ2xLiteScaler \
	HQ3xScaler HQ3xLiteScaler \
	RGBTriplet3xScaler MLAAScaler \
	Multiply32
endif

HDR_ONLY:= \
	LineScalers \
	Scaler \
	HQCommon

DIST:= \
	HQ2xScaler-1x1to2x2.nn \
	HQ2xScaler-1x1to1x2.nn \
	HQ3xScaler-1x1to3x3.nn \
	HQ2xLiteScaler-1x1to2x2.nn \
	HQ2xLiteScaler-1x1to1x2.nn \
	HQ3xLiteScaler-1x1to3x3.nn \
	LineScalers-x64.asm \
	LineScalers-x86.asm \
	Scale2xScaler-x64.asm \
	Scale2xScaler-x86.asm \
	Scanline-x64.asm \
	Scanline-x86.asm \
	Simple2xScaler-x64.asm \
	Simple2xScaler-x86.asm \
	Simple3xScaler-x64.asm \
	Simple3xScaler-x86.asm

SRC_HDR_$(COMPONENT_GL)+= \
	GLScalerFactory GLScaler \
	GLSimpleScaler GLScaleNxScaler GLSaIScaler GLTVScaler \
	GLRGBScaler GLHQScaler GLHQLiteScaler

include build/node-end.mk
