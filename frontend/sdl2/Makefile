include ../../config.mk

I_DIR:=.
O_DIR:=$(BUILD)/cache
LIB:=../../lib
BIN:=$(BUILD)/bin/$(NAME)

PKGS:=sdl2 SDL2_ttf fontconfig
override CFLAGS+= $(FLAGS) $(DEFINE) -I$(I_DIR) -I$(LIB) $(shell pkg-config --cflags $(PKGS))
LDFLAGS:=-L$(LIB)/$(BUILD) -l$(NAME) $(shell pkg-config --libs $(PKGS))

O_FILES:=$(O_DIR)/main.o              \
         $(O_DIR)/glyph_table.o       \
         $(O_DIR)/glyph_table/cache.o

$(BIN): $(O_FILES) ; @mkdir -p $(@D)
	$(CC) -o $(BIN) $^ $(LDFLAGS)

$(O_DIR)/%.o: $(I_DIR)/%.c $(I_DIR)/%.h ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | xargs -n1 > compile_flags.txt
