CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
VERBOSE ?= 0
TEST_ARGS := $(if $(filter 1,$(VERBOSE)),--verbose)

GAME_SOURCES := src/main.cpp src/game.cpp src/drawing.cpp
HEADERS := src/game.h src/drawing.h src/assets.h src/audio.h src/my_cases.h src/my_experiment.h
ifeq ($(OS),Windows_NT)
EXE := .exe
RAYLIB_CFLAGS ?=
RAYLIB_LIBS ?= -lraylib -lopengl32 -lgdi32 -lwinmm
GAME_FLAGS := -mwindows
else
EXE :=
RAYLIB_CFLAGS ?= $(shell pkg-config --cflags raylib)
RAYLIB_LIBS ?= $(shell pkg-config --libs raylib)
GAME_FLAGS :=
endif

GAME_OUT ?= build/game-l4$(EXE)
TEST_OUT ?= build/test-todos$(EXE)

.PHONY: all game run todo test clean
all: game
game: $(GAME_OUT)
$(GAME_OUT): $(GAME_SOURCES) $(HEADERS)
	mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) -Isrc $(GAME_SOURCES) -o "$@" $(GAME_FLAGS) $(RAYLIB_LIBS)
run: $(GAME_OUT)
	"$(abspath $(GAME_OUT))"
todo:
	@grep -nE '// TODO\(L4-[0-9][0-9](-[A-Z])?\):' src/game.cpp src/drawing.cpp src/my_experiment.h

# Compile on every test invocation: a failed compiler cannot run a stale binary.
# Fake device declarations are preincluded; fake.cpp is linked exactly once.
test: tests/test_todos.cpp tests/test_support.h tests/fake.cpp $(HEADERS) src/game.cpp src/drawing.cpp
	mkdir -p "$(dir $(TEST_OUT))"
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) -Isrc -include tests/test_support.h tests/test_todos.cpp tests/fake.cpp src/game.cpp src/drawing.cpp -o "$(TEST_OUT)" $(RAYLIB_LIBS)
	"$(abspath $(TEST_OUT))" $(TEST_ARGS)
clean:
	$(RM) "$(GAME_OUT)" "$(TEST_OUT)"

# Local-only extension is absent from the public student package.
-include teacher/Makefile
