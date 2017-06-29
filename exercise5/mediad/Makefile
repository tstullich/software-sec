TARGET := mediad
OBJ := mediad.o

HDR := mediad.h

INCL := $(shell pkg-config --cflags dbus-glib-1) -I../x-deps/libsafemalloc -I../x-deps/libmedialog
LIBS := $(shell pkg-config --libs dbus-glib-1) -L../x-deps/libsafemalloc -lsafemalloc

.PHONY: clean all

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -Wall -Wextra $(filter %.o,$^) -L. -L.. $(INCL) ${LIBS} -o $@

$(OBJ): %.o: %.c $(HDR)
	$(CC) $(CFLAGS) -g3 -Wall -Wextra $(INCL) $< -c -o $@

clean:
	rm -f $(TARGET) $(OBJ)
