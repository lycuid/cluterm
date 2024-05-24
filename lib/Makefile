include ../config.mk

IDIR:=.
ODIR:=$(BUILD)/cache
LIB:=$(BUILD)/lib/lib$(NAME).a

SRCS:=$(IDIR)/$(NAME).c            \
      $(IDIR)/$(NAME)/buffer.c     \
      $(IDIR)/$(NAME)/utf8.c       \
      $(IDIR)/$(NAME)/vte/parser.c \
      $(IDIR)/$(NAME)/vte/tty.c

OBJS:=$(SRCS:$(IDIR)/%.c=$(ODIR)/%.o)
override CFLAGS+= $(FLAGS) $(DEFINE) -fPIC -I$(IDIR)

$(LIB): $(OBJS) ; @mkdir -p $(@D)
	$(AR) rcs $@ $^

$(ODIR)/%.o: $(IDIR)/%.c $(IDIR)/%.h ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -fPIC -o $@ $<

$(ODIR)/$(NAME).o:              \
	$(IDIR)/$(NAME)/actions.h   \
	$(IDIR)/$(NAME)/actions/*.h \
	$(IDIR)/$(NAME)/colors.h

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | xargs -n1 > compile_flags.txt
