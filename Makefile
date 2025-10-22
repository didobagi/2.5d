UNAME := $(shell uname)

TARGET = build/main
SOURCES = src/main.c
OBJECTS = build/main.o
INCLUDES = -Iinclude

ifeq ($(UNAME), Darwin)
    CC = clang
    CFLAGS = $(INCLUDES) -x objective-c -DSOKOL_GLCORE
    LDFLAGS = -framework Cocoa -framework OpenGL -framework IOKit
else
    CC = cc
    CFLAGS = $(INCLUDES) -DSOKOL_GLCORE
    LDFLAGS = -lX11 -lXi -lXcursor -lGL -lpthread -lm -ldl
endif

all: build $(TARGET)

build:
	mkdir -p build

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

.PHONY: all clean
