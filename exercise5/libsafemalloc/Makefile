LIBTARGET := libsafemalloc.so
LIBOBJ := safemalloc.o

HDR := safemalloc.h

.PHONY: clean all

all: $(LIBTARGET)

$(LIBTARGET): $(LIBOBJ)
	$(LD) -shared --soname=libsafemalloc.so $^ -o $@

$(LIBOBJ): %.o: %.c
	$(CC) $(CFLAGS) -g3 -fPIC -Wall -Wextra $< -c -o $@

clean:
	rm -f $(LIBTARGET) $(LIBOBJ)
