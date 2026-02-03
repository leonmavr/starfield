CC ?= gcc

TARGET ?= starfield
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,%.o,$(SRC))

CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -pedantic

# Additional preprocessor flags:
# -DNO_X11 to disable X11 output
# -DDO_DITHER to enable dithering
# -DPPM to enable PPM output
# -DWIDTH=N and -DHEIGHT=N to set resolution
# -DNSTARS=N to set number of stars
CDEFS ?=

PKG_CONFIG ?= pkg-config
# Probe X11 only if NO_X11 is undefined
ifeq ($(filter -DNO_X11,$(CDEFS)),)
X11_CFLAGS := $(shell $(PKG_CONFIG) --cflags x11 2>/dev/null)
X11_LIBS   := $(shell $(PKG_CONFIG) --libs x11 2>/dev/null)
ifeq ($(strip $(X11_LIBS)),)
X11_LIBS = -lX11
endif
else
X11_CFLAGS :=
X11_LIBS   :=
endif

LDLIBS += -lm $(X11_LIBS)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(CDEFS) -o $@ $^ $(LDLIBS)

%.o: src/%.c
	$(CC) $(CFLAGS) $(CDEFS) $(X11_CFLAGS) -Isrc -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
