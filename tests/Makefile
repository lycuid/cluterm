include ../config.mk

LIB:=../lib
CFLAGS:=$(FLAGS) $(DEFINE) -I$(LIB) $(shell pkg-config --cflags criterion)
LDFLAGS:=$(shell pkg-config --libs criterion)

OBJS:=$(BUILD)/ctrl.o  \
      $(BUILD)/csi.o   \
      $(BUILD)/esc.o

test: build
	./$(BUILD)/tests

build: lib $(OBJS); @mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BUILD)/tests $(OBJS) -L$(LIB)/$(BUILD) -l$(NAME)

lib:
	$(MAKE) -j -C $(LIB)

$(BUILD)/%.o: %.c ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | xargs -n1 > compile_flags.txt
