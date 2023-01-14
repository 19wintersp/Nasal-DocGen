NAME = nasal-docgen

CC := clang

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%,obj/%.o,$(SRC))
BIN = $(NAME)

NASAL := nasal
NASAL_OBJ = $(wildcard $(NASAL)/*.c.o)

.PHONY:
.DEFAULT: $(BIN)

obj/%.o: src/%
	mkdir -p obj
	$(CC) -O2 -c $< -o $@

$(BIN): $(OBJ) $(NASAL_OBJ)
	$(CC) -O2 -lm $< $(NASAL_OBJ) -o $@
	strip $@
