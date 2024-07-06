CC = gcc
CFLAGS = -O0 -no-pie -ggdb -Wall -masm=intel -Wmissing-field-initializers -Wno-unused-function -Wno-unused-variable
CFLAGS_x86 = -m32 $(CFLAGS) # Compile 32 bit binary
DYNAMIC_CFLAGS = -Wl,-rpath . $(CFLAGS)
STATIC_CFLAGS = -static -Wl,-R . $(CFLAGS)
TARGET = mwe
TARGET_STATIC = $(TARGET)_static

ARCHIVE_FILE =  $(wildcard $(BUILD_DIR)/*.a)

LIBS = -l $(LIBARY_PATH)

SOURCES = $(wildcard *.c)
HEADERS = $(wildcard *.h) $(wildcard ucode/*.h)

$(TARGET_STATIC): $(SOURCES) $(HEADERS)
	$(CC) $(STATIC_CFLAGS) -I $(INCLUDE_DIR) $(SOURCES) $(ARCHIVE_FILE) -o $@

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(DYNAMIC_CFLAGS) -I $(INCLUDE_DIR) -L $(BUILD_DIR) $(SOURCES) -lmicro -o $@

rdrand_x86: rdrand_x86.c
	$(CC) $(CFLAGS_x86) rdrand_x86.c -o $@

all: $(TARGET_STATIC) $(TARGET)

clean:
	rm -f $(TARGET_STATIC) $(TARGET)

.PHONY: all clean
.DEFAULT_GOAL := all