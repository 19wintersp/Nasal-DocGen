NAME = nasal-docgen

CC := clang
CFLAGS = $(shell pkg-config --cflags libcmark)
LIBS = $(shell pkg-config --libs libcmark)

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%,obj/%.o,$(SRC))
BIN = $(NAME)

NASAL := nasal
NASAL_OBJ = $(wildcard $(NASAL)/*.c.o)

PREFIX := /usr/local

.PHONY: build install install_bin install_doc install_man
.DEFAULT: $(BIN)

build: $(BIN)

install: install_bin install_doc install_man

install_bin: $(BIN)
	cp $(BIN) $(PREFIX)/bin/

install_doc: license readme.md
	mkdir -p $(PREFIX)/doc/nasal-docgen
	cp license readme.md $(PREFIX)/doc/nasal-docgen/

install_man: man/nasal-docgen.1
	cp man/nasal-docgen.1 $(PREFIX)/share/man/man1/

obj/%.o: src/% include
	mkdir -p obj
	$(CC) -O2 -Iinclude $(CFLAGS) -DNAME=\"$(NAME)\" -c $< -o $@

$(BIN): $(OBJ) $(NASAL_OBJ)
	$(CC) -O2 -lm $(LIBS) $(OBJ) $(NASAL_OBJ) -o $@
	strip $@
