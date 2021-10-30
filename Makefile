CFLAGS = -std=c++17 -O2 -DNDEBUG=1 -I../stb
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
GLSL = glslangValidator

all: tutorial

.PHONY: test clean

tutorial: shades tutorial/bin/triangle

shades: tutorial/shaders/shader.vert tutorial/shaders/shader.frag
	$(GLSL) -V -o tutorial/shaders/vert.spv tutorial/shaders/shader.vert
	$(GLSL) -V -o tutorial/shaders/frag.spv tutorial/shaders/shader.frag

tutorial/bin/triangle: tutorial/triangle.cpp
	@mkdir -p tutorial/bin
	g++ $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -r bin
	rm -r tutorial/bin
