CFLAGS_ = -std=c++17 -O2 -DNDEBUG=1 -I../stb -g3
CFLAGS = -g3 -DDEBUG=1 -I../stb
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -lm
GLSL = glslangValidator
CC = gcc

all: template

.PHONY: clean


# tutorial

tutorial: tutorial/shaders/vert.spv tutorial/shaders/frag.spv tutorial/bin/triangle

tutorial/shaders/vert.spv: tutorial/shaders/shader.vert
	$(GLSL) -V -o $@ $^

tutorial/shaders/frag.spv: tutorial/shaders/shader.frag
	$(GLSL) -V -o $@ $^

tutorial/bin/triangle: tutorial/triangle.cpp
	@mkdir -p tutorial/bin
	g++ $(CFLAGS_) -o $@ $^ $(LDFLAGS)

# make sure the template compiles

template: template.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o build/$@.o $^

shaders: build/vert.spv build/frag.spv

build/frag.spv: shaders/shader.frag
	$(GLSL) -V -o $@ $^

build/vert.spv: shaders/shader.vert
	$(GLSL) -V -o $@ $^

test: template shaders
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/test build/*.o $(LDFLAGS)

clean:
	if [ -d bin ]; then rm -r bin; fi
	if [ -d build ]; then rm -r build; fi
	if [ -d tutorial/bin ]; then rm -r tutorial/bin; fi
