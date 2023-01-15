NAME = nasal-docgen

CC := clang
CFLAGS = $(shell pkg-config --cflags libcmark)
LIBS = $(shell pkg-config --libs libcmark)

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%,obj/%.o,$(SRC))
BIN = $(NAME)

NASAL := nasal
NASAL_OBJ = $(wildcard $(NASAL)/*.c.o)

.PHONY:
.DEFAULT: $(BIN)

obj/%.o: src/% include
	mkdir -p obj
	$(CC) -O2 -Iinclude $(CFLAGS) -DNAME=$(NAME) -c $< -o $@

$(BIN): $(OBJ) $(NASAL_OBJ)
	$(CC) -O2 -lm $(LIBS) $(OBJ) $(NASAL_OBJ) -o $@
	strip $@
