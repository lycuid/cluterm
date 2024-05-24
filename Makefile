include config.mk

IDIR:=src
BIN:=$(BUILD)/bin/$(NAME)

.PHONY: lib with_sdl2
with_sdl2: lib ; mkdir -p $(shell dirname $(BIN))
	$(MAKE) -j -C $(IDIR)/$@
	cp $(IDIR)/$@/$(BIN) $(BIN)

lib:
	$(MAKE) -j -C $@

.PHONY: run debug clean compile_flags fmt
run: ; ./$(BIN) 2>$(BUILD)/cluterm-err.txt | tee $(BUILD)/cluterm-out.txt
debug: $(BIN) ; lldb $(BIN)
clean: ; rm -rf $(BUILD)
	$(MAKE) -C lib $@
	$(MAKE) -C $(IDIR)/with_sdl2 $@
compile_flags:
	$(MAKE) -C lib $@
	$(MAKE) -C $(IDIR)/with_sdl2 $@
fmt: ; git ls-files | grep -E '\.[ch]$$' | xargs -i clang-format -i {}
