include ../../config.mk

IDIR:=.
ODIR:=$(BUILD)/cache
LIB:=../../lib
BIN:=$(BUILD)/bin/$(NAME)

PKGS:=sdl2 SDL2_ttf fontconfig
override CFLAGS+= $(FLAGS) $(DEFINE) -I$(IDIR) -I$(LIB) $(shell pkg-config --cflags $(PKGS))
LDFLAGS:=$(shell pkg-config --libs $(PKGS))

OBJS:=$(ODIR)/main.o              \
      $(ODIR)/glyph_table.o       \
      $(ODIR)/glyph_table/cache.o

$(BIN): $(OBJS) ; @mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $(BIN) $^ -L$(LIB)/$(BUILD) -l$(NAME)

$(ODIR)/%.o: $(IDIR)/%.c $(IDIR)/%.h ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | xargs -n1 > compile_flags.txt
