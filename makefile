NAME = nasal-docgen

CC := clang
CFLAGS = $(shell pkg-config --cflags libcmark libcjson libmustach)
LDFLAGS = -Xlinker --allow-multiple-definition
LIBS = $(shell pkg-config --libs libcmark libcjson libmustach)
DEFINES = -DNAME=\"$(NAME)\" -D_DEFAULT_SOURCE=1

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%,obj/%.o,$(SRC))
BIN = $(NAME)

NASAL := nasal
NASAL_OBJ = $(wildcard $(NASAL)/*.c.o)

PREFIX := /usr/local

.PHONY: build install install_bin install_doc install_man install_template
.DEFAULT: $(BIN)

build: $(BIN)

install: install_bin install_doc install_man install_template

install_bin: $(BIN)
	cp $(BIN) $(PREFIX)/bin/

install_doc: license readme.md
	mkdir -p $(PREFIX)/doc/nasal-docgen
	cp license readme.md $(PREFIX)/doc/nasal-docgen/

install_man: man/nasal-docgen.1
	cp man/nasal-docgen.1 $(PREFIX)/share/man/man1/

install_template: template
	mkdir -p $(PREFIX)/share/$(NAME)
	cp -r template $(PREFIX)/share/$(NAME)

obj/%.o: src/% include
	mkdir -p obj
	$(CC) -O2 -Iinclude $(CFLAGS) $(DEFINES) -c $< -o $@

$(BIN): $(OBJ) $(NASAL_OBJ)
	$(CC) -O2 -lm $(LDFLAGS) $(LIBS) $(OBJ) $(NASAL_OBJ) -o $@
	strip $@
