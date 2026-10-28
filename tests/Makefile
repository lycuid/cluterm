include ../config.mk

I_DIR:=.
O_DIR:=$(BUILD)/cache
LIB:=../lib
BIN:=$(BUILD)/bin/tests

PKGS:=criterion
override CFLAGS+= $(FLAGS) $(DEFINE) -I$(I_DIR) -I$(LIB) $(shell pkg-config --cflags $(PKGS))
override LDFLAGS+= -L$(LIB)/$(BUILD) -l$(NAME) $(shell pkg-config --libs $(PKGS))

O_FILES:=$(O_DIR)/vt_parser.o $(O_DIR)/utf8.o

run: $(BIN)
	./$(BIN) -j0

$(BIN): $(O_FILES) ; @mkdir -p $(@D)
	$(CC) -o $@ $(O_FILES) $(LDFLAGS)

$(O_DIR)/%.o: $(I_DIR)/%.c; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > compile_flags.txt
