include ../config.mk

I_DIR:=.
O_DIR:=$(BUILD)/cache
LIB:=$(BUILD)/lib$(NAME).a

O_FILES:=$(O_DIR)/$(NAME).o                 \
         $(O_DIR)/$(NAME)/colors.o          \
         $(O_DIR)/$(NAME)/handlers/key.o    \
         $(O_DIR)/$(NAME)/handlers/mouse.o  \
         $(O_DIR)/$(NAME)/utf8.o            \
         $(O_DIR)/$(NAME)/vt/buffer.o       \
         $(O_DIR)/$(NAME)/vt/buffer_line.o  \
         $(O_DIR)/$(NAME)/vt/parser.o       \
         $(O_DIR)/$(NAME)/vt/selection.o

override CFLAGS+= $(FLAGS) $(DEFINE) -I$(I_DIR)

$(LIB): $(O_FILES); @mkdir -p $(@D)
	$(AR) rcs $@ $^

$(O_DIR)/%.o: $(I_DIR)/%.c $(I_DIR)/%.h ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(O_FILES): $(I_DIR)/$(NAME)/vt/fsm.h

$(I_DIR)/$(NAME)/vt/fsm.h: $(I_DIR)/$(NAME)/vt/fsm.def
	python $(I_DIR)/$(NAME)/vt/fsm_gen.py < $< > $@

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > compile_flags.txt
