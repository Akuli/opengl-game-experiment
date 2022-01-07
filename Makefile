CFLAGS += -Wall -Wextra -Wpedantic -std=c11
CFLAGS += -Wfloat-conversion -Wno-sign-compare -Werror=int-conversion
CFLAGS += -Werror=incompatible-pointer-types
CFLAGS += -Werror=implicit-function-declaration
CFLAGS += -Werror=discarded-qualifiers
CFLAGS += -Werror=stack-usage=60000
CFLAGS += -DSDL_ASSERT_LEVEL=2              # enable SDL_assert()
#CFLAGS += -Ofast -fno-finite-math-only  # https://stackoverflow.com/q/47703436
CFLAGS += -g
CFLAGS += -MMD
LDFLAGS += -lm -lSDL2

CFLAGS += $(shell pkg-config glew --cflags)
LDFLAGS += $(shell pkg-config glew --libs)

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=obj/%.o)
DEPENDS = $(OBJ:%.o=%.d)

all: game

game: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

obj/%.o: src/%.c
	mkdir -p $(@D) && $(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -rf obj
	rm -f game

-include $(DEPENDS)
