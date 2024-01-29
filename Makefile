NAME:=cluterm
VERSION:=0.1.0
IDIR:=src
BUILD:=.build
ODIR:=$(BUILD)/cache
BIN:=$(BUILD)/bin/$(NAME)
SRCS:=$(IDIR)/$(NAME).c                             \
      $(IDIR)/$(NAME)/gfx.c                         \
      $(IDIR)/$(NAME)/gfx/glyph_table.c             \
      $(IDIR)/$(NAME)/gfx/glyph_table/cache.c       \
      $(IDIR)/$(NAME)/terminal.c                    \
      $(IDIR)/$(NAME)/terminal/buffer.c             \
      $(IDIR)/$(NAME)/utf8.c                        \
      $(IDIR)/$(NAME)/vte/parser.c                  \
      $(IDIR)/$(NAME)/vte/tty.c
OBJS:=$(SRCS:$(IDIR)/%.c=$(ODIR)/%.o)
PKGS=sdl2 SDL2_ttf fontconfig
DEFINE:=-DNAME='"$(NAME)"' -DVERSION='"$(VERSION)"' -D_XOPEN_SOURCE=700
FLAGS:=-Wall -Wextra -Wvla -I$(IDIR) -pedantic -std=c99 -ggdb -Ofast
override CFLAGS+= $(FLAGS) $(DEFINE) $(shell pkg-config --cflags $(PKGS))
override LDFLAGS+= $(shell pkg-config --libs $(PKGS))

define COMPILE =
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
endef

$(BIN): $(OBJS)
		@mkdir -p $(@D)
		$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(OBJS): $(IDIR)/config.h $(IDIR)/$(NAME)/utils.h

$(ODIR)/$(NAME)/terminal.o:                   \
	$(IDIR)/$(NAME)/terminal/actions/ctrl.h   \
	$(IDIR)/$(NAME)/terminal/actions/esc.h    \
	$(IDIR)/$(NAME)/terminal/actions/csi.h    \
	$(IDIR)/$(NAME)/terminal/actions.h        \
	$(IDIR)/$(NAME)/terminal/colors.h

$(ODIR)/$(NAME)/terminal/buffer.o: $(IDIR)/$(NAME)/terminal/colors.h

$(ODIR)/%.o: $(IDIR)/%.c $(IDIR)/%.h ; $(COMPILE)
$(ODIR)/%.o: $(IDIR)/%.c ;             $(COMPILE)

.PHONY: run debug clean compile_flags fmt
# run: $(BIN) ; lldb -o run ./$(BIN) 2>.build/cluterm-err.txt
run: $(BIN) ; ./$(BIN) 2>$(BUILD)/cluterm-err.txt | tee $(BUILD)/cluterm-out.txt
debug: $(BIN) ; lldb $(BIN)
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | xargs -n1 > compile_flags.txt
fmt: ; git ls-files | grep -E '\.[ch]$$' | xargs -i clang-format -i {}
