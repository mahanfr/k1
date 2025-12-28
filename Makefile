CC = gcc
CFLAGS = -Wall -Wextra -pedantic
INCLUDE_PATHS =
ifeq ($(OS),Windows_NT)
    LDFLAGS =
else
    LDFLAGS =
endif
ifeq ($(OS),Windows_NT)
	LDLIBS =
else
	LDLIBS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
endif
SRC_DIR = src
BUILD_DIR := build
TARGET_NAME := k1vulk
TARGET_BIN = $(BUILD_DIR)/$(TARGET_NAME)
TARGET_DLIB = $(BUILD_DIR)/lib$(TARGET_NAME).so
TARGET_SLIB = $(BUILD_DIR)/lib$(TARGET_NAME).a

rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
SRC = $(call rwildcard,$(SRC_DIR),*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

.PHONY: all library bin clean

all: always library bin

library: always $(TARGET_DLIB)

bin: always $(TARGET_BIN)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) -fPIC -c $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(TARGET_BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
	@ echo DONE. CREATED $@

$(TARGET_DLIB): $(OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS) -DLIB
	@ echo DONE. CREATED Dynamic Library $@

always:
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
