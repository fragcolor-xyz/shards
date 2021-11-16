CURDIR := $(dir $(lastword $(MAKEFILE_LIST)))

VS_FLAGS=-i $(CURDIR)/include
FS_FLAGS=-i $(CURDIR)/include
CS_FLAGS=-i $(CURDIR)/include

BUILD_DIR=$(CURDIR)/build
RUNTIME_DIR=$(CURDIR)/../built
ADDITIONAL_INCLUDES=-i $(CURDIR)/include
export
