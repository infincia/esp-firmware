#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := firmware

GIT_VERSION := $(shell git describe --always --tags)
WIFI_SSID := $(FIRMWARE_WIFI_SSID)
WIFI_PASSWORD := $(FIRMWARE_WIFI_PASSWORD)

CPPFLAGS := -DVERSION=\"$(GIT_VERSION)\" -DFIRMWARE_WIFI_SSID=\"$(WIFI_SSID)\" -DFIRMWARE_WIFI_PASSWORD=\"$(WIFI_PASSWORD)\"

include $(IDF_PATH)/make/project.mk
