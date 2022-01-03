CFLAGS += -Wall -Wextra -Wpedantic -std=c11
CFLAGS += -Wfloat-conversion -Wno-sign-compare -Werror=int-conversion
CFLAGS += -Werror=incompatible-pointer-types
CFLAGS += -Werror=implicit-function-declaration
CFLAGS += -Werror=discarded-qualifiers
CFLAGS += -Werror=stack-usage=60000
CFLAGS += -Ofast -fno-finite-math-only  # https://stackoverflow.com/q/47703436
CFLAGS += -MMD
LDFLAGS += -lm -lSDL2

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=obj/%.o)
DEPENDS = $(OBJ:%.o=%.d)

all: game

game: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

obj/%.o: src/%.c
	mkdir -p $(@D) && $(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -r obj
	rm game

-include $(DEPENDS)
