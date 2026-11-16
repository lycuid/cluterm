include ../../config.mk

I_DIR:=.
O_DIR:=$(BUILD)/cache
LIB:=../../lib
SHR:=../shared
BIN:=$(BUILD)/bin/$(NAME)

PKGS:=sdl2 SDL2_ttf fontconfig
override CFLAGS+= $(FLAGS) $(DEFINE) -I$(I_DIR) -I$(SHR) -I$(LIB)/include $(shell pkg-config --cflags $(PKGS))
override LDFLAGS+= -L$(LIB)/$(BUILD) -l$(NAME) $(shell pkg-config --libs $(PKGS)) -lm

O_FILES:=$(O_DIR)/main.o                   \
         $(O_DIR)/frame.o                  \
         $(O_DIR)/glyph_cache.o            \
         $(O_DIR)/glyph_cache/lru.o        \
         $(O_DIR)/handlers/keypress.o      \
         $(SHR)/$(O_DIR)/args.o            \
         $(SHR)/$(O_DIR)/pty.o

$(BIN): $(O_FILES) ; @mkdir -p $(@D)
	$(CC) -o $@ $^ $(LDFLAGS)

$(O_DIR)/%.o: $(I_DIR)/%.c $(I_DIR)/%.h ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(SHR)/%: ;
	$(MAKE) -C $(SHR) $*

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > compile_flags.txt
