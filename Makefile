# Board setup
BOARD_TAG    = nano
BOARD_SUB    = atmega328

# Use 'do_upload' target to upload software to arduino's flash.
# You need to set this variable correctly.
MONITOR_PORT ?= /dev/ttyUSB0

# Libs for handling the RTC and WS2812
ARDUINO_LIBS = DS3231M FastLED Wire

# Points to submodule
ARDMK_DIR:=$(PWD)/Arduino-Makefile

# You may need to change this, depending on your local setup.
ARDUINO_DIR ?= $(HOME)/arduino/arduino-1.8.12

USER_LIB_PATH := $(PWD)

# In case we need it
# CFLAGS += -ffast-math
# CXXFLAGS += -ffast-math

include $(ARDMK_DIR)/Arduino.mk
