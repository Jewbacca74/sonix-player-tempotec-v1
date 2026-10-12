CC ?= cc
PKG_CONFIG ?= pkg-config
ROOT := ..
BUILD := .build
CPPFLAGS += -I$(ROOT)
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 -ffunction-sections -fdata-sections
LDLIBS += -lpthread -ldl -lm
LDFLAGS += -Wl,--gc-sections

CORE = $(ROOT)/src/system/streaming/subsonic.c \
       $(ROOT)/src/system/streaming/streambridge.c \
       $(ROOT)/src/system/streaming/subsonicstream.c \
       $(ROOT)/src/system/net/http.c $(ROOT)/src/system/net/tls.c \
       $(ROOT)/src/system/core/json.c $(ROOT)/src/system/core/md5.c \
       $(ROOT)/src/system/core/config.c $(ROOT)/src/system/playback/playlist.c

.PHONY: test sanitize clean

test: $(BUILD)/test_subsonic
	python3 run_subsonic_tests.py $(BUILD)/test_subsonic

$(BUILD)/test_subsonic: test_subsonic.c support.c $(CORE) $(shell find $(ROOT)/src/system -name '*.h')
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_subsonic.c support.c $(CORE) $(LDFLAGS) $(LDLIBS)

sanitize:
	$(MAKE) BUILD=.build/sanitize CFLAGS='-O1 -g -std=gnu11 -Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 -ffunction-sections -fdata-sections -fno-omit-frame-pointer -fsanitize=address,undefined' LDFLAGS='-Wl,--gc-sections -fsanitize=address,undefined' test

# Optional real-codec integration: the usual host development packages are
# sufficient. No fake MP3/FLAC/Opus decoder is used. See tests/README.md.
CODEC = $(filter-out %/wavpackdec.c %/opusdec.c,$(wildcard $(ROOT)/src/system/decode/*.c)) \
        $(ROOT)/src/system/decode/opusdec.c $(ROOT)/src/system/decode/wavpackdec.c \
        $(ROOT)/src/system/decode/alac/alac.c $(ROOT)/src/system/decode/ape/apecore.c \
        $(ROOT)/src/system/library/cue.c
.PHONY: codecs sanitize-codecs
codecs: $(BUILD)/test_codecs
	python3 run_subsonic_tests.py $(BUILD)/test_codecs

$(BUILD)/test_codecs: test_codecs.c support.c $(CORE) $(CODEC)
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DTEST_REAL_CODECS=1 $(shell $(PKG_CONFIG) --cflags opusfile wavpack) \
		-o $@ test_codecs.c support.c $(CORE) $(CODEC) $(LDFLAGS) $(LDLIBS) $(shell $(PKG_CONFIG) --static --libs opusfile wavpack)

# Existing MP4 tag copies intentionally truncate display strings; GCC -O1
# diagnoses those inlined bounded copies. Keep all other warnings fatal.
sanitize-codecs:
	$(MAKE) BUILD=.build/sanitize-codecs CFLAGS='-O1 -g -std=gnu11 -Wall -Wextra -Werror -Wno-error=format-truncation -D_FILE_OFFSET_BITS=64 -ffunction-sections -fdata-sections -fno-omit-frame-pointer -fsanitize=address,undefined' LDFLAGS='-Wl,--gc-sections -fsanitize=address,undefined' codecs

clean:
	rm -rf $(BUILD)
