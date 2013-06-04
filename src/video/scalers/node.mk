include build/node-start.mk

SRC_HDR:= \
	ScalerFactory \
	Scaler1 \
	DirectScalerOutput SuperImposeScalerOutput StretchScalerOutput

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
	Scaler ScalerOutput \
	HQCommon

DIST:= \
	HQ2xScaler-1x1to2x2.nn \
	HQ2xScaler-1x1to1x2.nn \
	HQ3xScaler-1x1to3x3.nn \
	HQ2xLiteScaler-1x1to2x2.nn \
	HQ2xLiteScaler-1x1to1x2.nn \
	HQ3xLiteScaler-1x1to3x3.nn \

SRC_HDR_$(COMPONENT_GL)+= \
	GLScalerFactory GLScaler \
	GLSimpleScaler GLScaleNxScaler GLSaIScaler GLTVScaler \
	GLRGBScaler GLHQScaler GLHQLiteScaler

include build/node-end.mk
