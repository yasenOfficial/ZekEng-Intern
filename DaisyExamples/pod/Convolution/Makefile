TARGET = Convolution

USE_DAISYSP_LGPL=1
SD_CARD=1
USE_FATFS=1

# Sources
CPP_SOURCES = src/main.cpp src/WavFile.cpp

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP

# Add 'inc' as an include path
C_INCLUDES = \
-Iinc

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
