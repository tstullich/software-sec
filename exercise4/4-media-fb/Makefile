TARGET := mediacenter
INCL := -I../x-deps/libmedialog
LIBS := -L../x-deps/libmedialog -lmedialog -lmagic -lpthread
OBJ := main.o mev.o content.o scan.o misc.o network.o notify.o
HDR := mediacenter.h

.PHONY: clean all

# CONFIG_CONFIGURABLE_HANDLERS:
#   handler list is allocated dynamically and thus modifiable

# simple
MEDIACENTER_CFLAGS := -DCONFIG_CONFIGURABLE_HANDLERS
# hard
## MEDIACENTER_CFLAGS :=

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -Wall -Wextra $^ $(LIBS) -o $@

$(OBJ): %.o: %.c $(HDR)
	$(CC) $(CFLAGS) -Wall -Wextra $(MEDIACENTER_CFLAGS) $(INCL) $< -g3 -c -o $@

clean:
	rm -f $(TARGET) $(OBJ)
