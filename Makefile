# -------- basic settings --------
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20

# include path (only needed if you have local headers)
INCLUDES = -IGame

# glfw flags from pkg-config
GLFW_CFLAGS = $(shell pkg-config --cflags glfw3)
GLFW_LIBS   = $(shell pkg-config --libs glfw3)

# vulkan flags from pkg-config
VULKAN_CFLAGS = $(shell pkg-config --cflags vulkan)
VULKAN_LIBS   = $(shell pkg-config --libs vulkan)

# libraries
LIBS = $(GLFW_LIBS) $(VULKAN_LIBS)

# files
SRC = Engine/main.cpp
OUT = build/app
OUT_DEBUG = build/debug

# -------- rules --------
all: release

release: CXXFLAGS += -O2 -DNDEBUG
release: $(OUT)

debug: CXXFLAGS += -g -O0
debug: $(OUT_DEBUG)

$(OUT): $(SRC)
	mkdir -p build
	./.compileShaders
	$(CXX) $(CXXFLAGS) $(SRC) $(INCLUDES) $(GLFW_CFLAGS) $(VULKAN_CFLAGS) $(LIBS) -o $(OUT)


$(OUT_DEBUG): $(SRC)
	mkdir -p build
	./.compileShaders
	$(CXX) $(CXXFLAGS) $(SRC) $(INCLUDES) $(GLFW_CFLAGS) $(VULKAN_CFLAGS) $(LIBS) -o $(OUT_DEBUG)

clean:
	rm -rf build

shaders:
	./.compileShaders

.PHONY: all release debug clean shaders
