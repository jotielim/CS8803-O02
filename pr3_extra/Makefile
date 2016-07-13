CFLAGS := -Wall --std=gnu99 -g3 -Wno-format-security -Werror -fsanitize=address -fno-omit-frame-pointer
CURL_LIBS := $(shell curl-config --libs)
CURL_CFLAGS := $(shell curl-config --cflags)

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread -lrt -static-libasan
endif

PROXY_OBJ := webproxy.o steque.o

all: gfclient_download webproxy simplecached

gfclient_download: gfclient.o workload.o gfclient_download.o steque.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

webproxy: $(PROXY_OBJ) handler.o content.o handle_with_cache.o handle_with_curl.o shm_channel.o gfserver.o
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

simplecached: simplecache.o simplecached.o shm_channel.o steque.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

.PHONY: clean

clean:
	rm -rf *.o webproxy simplecached gfclient_download
