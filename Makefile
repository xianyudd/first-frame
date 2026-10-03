CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
VERBOSE ?= 0
TEST_ARGS := $(if $(filter 1,$(VERBOSE)),--verbose)
HEADERS := $(wildcard src/*.h)
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

GAME_OUT ?= build/game$(EXE)
.PHONY: all game scaffold run todo test clean
all: game
game scaffold: $(GAME_OUT)
build:
	mkdir -p build
$(GAME_OUT): src/main.cpp $(HEADERS) | build
	mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) $< -o "$@" $(GAME_FLAGS) $(RAYLIB_LIBS)
run: $(GAME_OUT)
	"$(abspath $(GAME_OUT))"

# Live task locations: ten tasks, twelve unique markers.
todo:
	@grep -nE '// TODO\(L3-[0-9][0-9](-[A-Z])?\):' src/main.cpp src/my_cases.h

# Console-only headless checks; real failures stop make.
test: src/main.cpp tests/test_todos.cpp $(HEADERS) tests/fake_audio_backend.h | build
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) tests/test_todos.cpp -o build/test-todos$(EXE) $(RAYLIB_LIBS)
	./build/test-todos$(EXE) $(TEST_ARGS)
clean:
	$(RM) "$(GAME_OUT)" build/test-todos$(EXE)
