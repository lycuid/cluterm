include ../config.mk

I_DIR:=.
O_DIR:=$(BUILD)/cache
LIB:=$(BUILD)/lib$(NAME).a

O_FILES:=$(O_DIR)/$(NAME).o            \
         $(O_DIR)/$(NAME)/config.o     \
         $(O_DIR)/$(NAME)/pty.o        \
         $(O_DIR)/$(NAME)/utf8.o       \
         $(O_DIR)/$(NAME)/vt/buffer.o  \
         $(O_DIR)/$(NAME)/vt/parser.o

override CFLAGS+= $(FLAGS) $(DEFINE) -fPIC -I$(I_DIR)

$(LIB): $(O_FILES) ; @mkdir -p $(@D)
	$(AR) rcs $@ $^

$(O_DIR)/%.o: $(I_DIR)/%.c $(I_DIR)/%.h ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -fPIC -o $@ $<

$(O_DIR)/$(NAME).o:                  \
	$(I_DIR)/$(NAME)/colors.h          \
	$(I_DIR)/$(NAME)/scanner.h         \
	$(I_DIR)/$(NAME)/util.h            \
	$(I_DIR)/$(NAME)/vt/actions.h      \
	$(I_DIR)/$(NAME)/vt/actions/esc.h  \
	$(I_DIR)/$(NAME)/vt/actions/csi.h  \
	$(I_DIR)/$(NAME)/vt/actions/ctrl.h

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > compile_flags.txt
