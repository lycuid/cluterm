include ../config.mk

LIB:=../lib
CFLAGS:=$(FLAGS) $(DEFINE) -I$(LIB) $(shell pkg-config --cflags criterion)
LDFLAGS:=-L$(LIB)/$(BUILD) -l$(NAME) $(shell pkg-config --libs criterion)

O_FILES:=$(BUILD)/ctrl.o  \
         $(BUILD)/csi.o   \
         $(BUILD)/esc.o

test: build
	./$(BUILD)/tests

build: lib $(O_FILES); @mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/tests $(O_FILES) $(LDFLAGS)

lib:
	$(MAKE) -j -C $(LIB)

$(BUILD)/%.o: %.c ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | xargs -n1 > compile_flags.txt
