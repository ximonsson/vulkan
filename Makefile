CFLAGS = -std=c++17 -O2 -DNDEBUG=1 -I../stb
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
GLSL = glslangValidator

all: shades triangle

.PHONY: test clean

shades: shaders/shader.vert shaders/shader.frag
	$(GLSL) -V -o shaders/vert.spv shaders/shader.vert
	$(GLSL) -V -o shaders/frag.spv shaders/shader.frag

triangle: triangle.cpp
	g++ $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm triangle
