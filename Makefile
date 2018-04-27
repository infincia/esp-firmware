#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := firmware

GIT_VERSION := $(shell git describe --always --tags)
CPPFLAGS := -DVERSION=\"$(GIT_VERSION)\"

include $(IDF_PATH)/make/project.mk
