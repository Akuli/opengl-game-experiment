CXXFLAGS += -Wall -Wextra -Wpedantic -std=c++17
CXXFLAGS += -Wfloat-conversion -Wno-sign-compare
CXXFLAGS += -Wno-missing-field-initializers
CXXFLAGS += -Werror=stack-usage=60000
CXXFLAGS += -DSDL_ASSERT_LEVEL=2              # enable SDL_assert()
#CXXFLAGS += -Ofast -fno-finite-math-only  # https://stackoverflow.com/q/47703436
CXXFLAGS += -g
CXXFLAGS += -MMD
LDFLAGS += -lm -lSDL2

CXXFLAGS += $(shell pkg-config glew --cflags)
LDFLAGS += $(shell pkg-config glew --libs)

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=obj/%.o)
DEPENDS = $(OBJ:%.o=%.d)

all: game

game: $(OBJ)
	$(CC) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS)

obj/%.o: src/%.cpp
	mkdir -p $(@D) && $(CC) -c -o $@ $< $(CXXFLAGS)

clean:
	rm -rf obj
	rm -f game

.PHONY: iwyu
iwyu:
	$(MAKE) -f Makefile.iwyu

-include $(DEPENDS)
