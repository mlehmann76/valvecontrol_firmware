#
# Main Makefile. This is basically the same as a component makefile.
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component_common.mk. By default, 
# this will take the sources in the src/ directory, compile them and link them into 
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#
$(eval GIT_BRANCH=$(shell git describe --tags))

CXXFLAGS += -std=c++17
COMPONENT_ADD_INCLUDEDIRS := . ../components/FreeRTOScpp ../components/http ../components/json/include/nlohmann ../components/variant/include
COMPONENT_SRCDIRS := $(COMPONENT_ADD_INCLUDEDIRS) 
CPPFLAGS += -DPROJECT_GIT='"'$(GIT_BRANCH)'"'
COMPONENT_EMBED_TXTFILES := config.json

	
	
