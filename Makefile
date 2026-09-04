include config.mk

FRONTEND:=frontend
BIN:=$(BUILD)/bin/$(NAME)

build: ;
	$(MAKE) $(FRONTEND)/sdl2 \
		CFLAGS="-DDEBUG_LVL=0 -O3 $(CFLAGS)"

debug: ;
	$(MAKE) $(FRONTEND)/sdl2 \
		CFLAGS="-DDEBUG_LVL=2 -fsanitize=undefined,address -ggdb -O0 $(CFLAGS)" \
		LDFLAGS="-fsanitize=undefined,address $(LDFLAGS)"

thread-debug: ;
	$(MAKE) $(FRONTEND)/sdl2 \
		CFLAGS="-DDEBUG_LVL=2 -fsanitize=thread -ggdb -O0 $(CFLAGS)" \
		LDFLAGS="-fsanitize=thread $(LDFLAGS)"

.PHONY: $(FRONTEND)/sdl2
$(FRONTEND)/sdl2: lib ; mkdir -p $(shell dirname $(BIN))
	$(MAKE) -j -C $@
	cp $@/$(BIN) $(BIN)

.PHONY: lib
lib:
	$(MAKE) -j -C $@

.PHONY: run clean compile_flags fmt
run: ; ./$(BIN) 2>&1 | tee cluterm-out.txt

clean: ; rm -rf $(BUILD)
	$(MAKE) -C lib $@
	$(MAKE) -C $(FRONTEND)/sdl2 $@
compile_flags:
	$(MAKE) -C lib $@
	$(MAKE) -C $(FRONTEND)/sdl2 $@
fmt: ; git ls-files | grep -E '\.[ch]$$' | xargs -i clang-format -i {}
