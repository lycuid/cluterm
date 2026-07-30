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
run: ; ./$(BIN) | tee $(BUILD)/cluterm-out.txt

build: ;
	$(MAKE) CFLAGS="-DDEBUG_LVL=0 -Ofast"

debug-build: ;
	$(MAKE) \
		CFLAGS="-DDEBUG_LVL=1 -D_COLORS__VSCODE -fsanitize=address -ggdb -O0" \
		LDFLAGS="-fsanitize=address"

clean: ; rm -rf $(BUILD)
	$(MAKE) -C lib $@
	$(MAKE) -C tests $@
	$(MAKE) -C $(FRONTEND)/sdl2 $@
compile_flags:
	$(MAKE) -C lib $@
	$(MAKE) -C tests $@
	$(MAKE) -C $(FRONTEND)/sdl2 $@
fmt: ; git ls-files | grep -E '\.[ch]$$' | xargs -i clang-format -i {}
