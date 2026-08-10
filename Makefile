include config.mk

FRONTEND:=frontend
BIN:=$(BUILD)/bin/$(NAME)

.PHONY: $(FRONTEND)/sdl2
$(FRONTEND)/sdl2: lib ; mkdir -p $(shell dirname $(BIN))
	$(MAKE) -j -C $@
	cp $@/$(BIN) $(BIN)

.PHONY: lib
lib:
	$(MAKE) -j -C $@

.PHONY: tests
tests:
	$(MAKE) -j -C $@

.PHONY: run debug clean compile_flags fmt
run: ; ./$(BIN)

build: ;
	$(MAKE) CFLAGS="-DDEBUG_LVL=0 -O3"

debug-build: ;
	$(MAKE) \
		CFLAGS="-DDEBUG_LVL=1 -DPALETTE_VSCODE -fsanitize=undefined,address -ggdb -O0" \
		LDFLAGS="-fsanitize=undefined,address"

clean: ; rm -rf $(BUILD)
	$(MAKE) -C lib $@
	$(MAKE) -C tests $@
	$(MAKE) -C $(FRONTEND)/sdl2 $@
compile_flags:
	$(MAKE) -C lib $@
	$(MAKE) -C tests $@
	$(MAKE) -C $(FRONTEND)/sdl2 $@
fmt: ; git ls-files | grep -E '\.[ch]$$' | xargs -i clang-format -i {}
