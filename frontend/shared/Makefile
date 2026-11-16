include ../../config.mk

I_DIR:=.
O_DIR:=$(BUILD)/cache

override CFLAGS+= $(FLAGS) $(DEFINE) -D_GNU_SOURCE -I$(I_DIR) -I../../lib/include

$(O_DIR)/%.o: $(I_DIR)/%.c $(I_DIR)/%.h ; @mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean compile_flags
clean: ; rm -rf $(BUILD)
compile_flags: ; @echo $(CFLAGS) | tr ' ' '\n' > compile_flags.txt
