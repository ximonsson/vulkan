CFLAGS = -std=c++17 -O2 -DNDEBUG=1 -I../stb
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
GLSL = glslangValidator

all: template shades triangle

.PHONY: test clean

template: main.cpp
	g++ $(CFLAGS) -o $@ $^ $(LDFLAGS)

triangle: triangle.cpp
	g++ $(CFLAGS) -o $@ $^ $(LDFLAGS)

shades: shaders/shader.vert shaders/shader.frag
	$(GLSL) -V -o shaders/vert.spv shaders/shader.vert
	$(GLSL) -V -o shaders/frag.spv shaders/shader.frag

test: all
	./vulkan

clean:
	rm -f vulkan
