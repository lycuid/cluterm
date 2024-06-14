include ../config.mk

I_DIR:=.
O_DIR:=$(BUILD)/cache
LIB:=$(BUILD)/lib$(NAME).a

O_FILES:=$(O_DIR)/$(NAME).o            \
         $(O_DIR)/$(NAME)/buffer.o     \
         $(O_DIR)/$(NAME)/utf8.o       \
         $(O_DIR)/$(NAME)/vte/parser.o \
         $(O_DIR)/$(NAME)/vte/tty.o

override CFLAGS+= $(FLAGS) $(DEFINE) -fPIC -I$(I_DIR)

$(LIB): $(O_FILES) ; @mkdir -p $(@D)
	$(AR) rcs $@ $^

$(O_DIR)/%.o: $(I_DIR)/%.c $(I_DIR)/%.h ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -fPIC -o $@ $<

$(O_DIR)/$(NAME).o:               \
	$(I_DIR)/$(NAME)/actions.h      \
	$(I_DIR)/$(NAME)/actions/csi.h  \
	$(I_DIR)/$(NAME)/actions/ctrl.h \
	$(I_DIR)/$(NAME)/actions/esc.h  \
	$(I_DIR)/$(NAME)/actions/osc.h  \
	$(I_DIR)/$(NAME)/colors.h

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | xargs -n1 > compile_flags.txt
