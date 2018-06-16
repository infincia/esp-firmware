#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := firmware

FIRMWARE_VERSION := $(shell git describe --always --tags --abbrev=0)
F := $(shell echo "\#include \"pch.hpp\"\n\nconst char *FIRMWARE_VERSION = \"$(FIRMWARE_VERSION)\";" > main/version.cpp)
WIFI_SSID := $(FIRMWARE_WIFI_SSID)
WIFI_PASSWORD := $(FIRMWARE_WIFI_PASSWORD)

CPPFLAGS := -DFIRMWARE_WIFI_SSID=\"$(WIFI_SSID)\" -DFIRMWARE_WIFI_PASSWORD=\"$(WIFI_PASSWORD)\"

include $(IDF_PATH)/make/project.mk
