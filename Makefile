UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
    DEFINES = -DSOKOL_GLCORE
    LIBS = -lX11 -lXi -lXcursor -lGL -lpthread -lm -ldl
endif

ifeq ($(UNAME), Darwin)
	DEFINES = -DSOKOL_METAL
	LIBS = -framework Cocoa -framework Metal -framework MetalKit -framework Quartz
endif

CFLAGS = $(DEFINES) -Iinclude
LDFLAGS = $(LIBS)
SRCDIR = src
BUILDDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
TARGET = build/main

all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) 

