include ../config.mk

I_DIR:=.
O_DIR:=$(BUILD)/cache
LIB:=../lib
BIN:=$(BUILD)/bin/vt_parser_test
LIBRARY:=$(LIB)/$(BUILD)/lib$(NAME).a

PKGS:=criterion
override CFLAGS+= $(FLAGS) $(DEFINE) -I$(I_DIR) -I$(LIB) -I$(LIB)/include $(shell pkg-config --cflags $(PKGS))
override LDFLAGS+= -L$(LIB)/$(BUILD) -l$(NAME) $(shell pkg-config --libs $(PKGS))

O_FILES:=$(O_DIR)/vt_parser.o

all: $(BIN)
	@echo "Built $(BIN)"

$(BIN): $(O_FILES) $(LIBRARY) ; @mkdir -p $(@D)
	$(CC) -o $@ $(O_FILES) $(LDFLAGS)

$(O_DIR)/vt_parser.o: $(I_DIR)/vt_parser.c $(LIB)/cluterm/vt/parser.h ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIBRARY): FORCE_LIB
	$(MAKE) -C $(LIB)

.PHONY: run clean compile_flags FORCE_LIB
FORCE_LIB:

run: $(BIN)
	./$(BIN) --jobs 1

clean: ; rm -rf $(BUILD)

compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > compile_flags.txt
