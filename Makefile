include config.mk

BIN:=$(BUILD)/bin/$(NAME)

build: ;
	$(MAKE) frontend/sdl2 \
		CFLAGS="-DDEBUG_LVL=0 -O3 $(CFLAGS)"

debug: ;
	$(MAKE) frontend/sdl2 \
		CFLAGS="-DDEBUG_LVL=2 -fsanitize=undefined,address -ggdb -O0 $(CFLAGS)" \
		LDFLAGS="-fsanitize=undefined,address $(LDFLAGS)"

thread-debug: ;
	$(MAKE) frontend/sdl2 \
		CFLAGS="-DDEBUG_LVL=2 -fsanitize=thread -ggdb -O0 $(CFLAGS)" \
		LDFLAGS="-fsanitize=thread $(LDFLAGS)"

.PHONY: frontend/sdl2
frontend/sdl2: lib ; mkdir -p $(shell dirname $(BIN))
	$(MAKE) -C $@
	cp $@/$(BIN) $(BIN)

.PHONY: lib
lib:
	$(MAKE) -C $@

.PHONY: lib/tests
lib/tests: lib;
	$(MAKE) -C $@

.PHONY: run clean compile_flags fmt
run: ; ./$(BIN) 2>&1 | tee cluterm-out.txt

clean: ; rm -rf $(BUILD)
	$(MAKE) -C lib $@
	$(MAKE) -C lib/tests $@
	$(MAKE) -C frontend/sdl2 $@
	$(MAKE) -C frontend/shared $@
compile_flags:
	$(MAKE) -C lib $@
	$(MAKE) -C lib/tests $@
	$(MAKE) -C frontend/sdl2 $@
fmt: ; git ls-files | grep -E '\.[ch]$$' | xargs -i clang-format -i {}
