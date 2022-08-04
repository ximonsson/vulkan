CFLAGS_ = -std=c++17 -O2 -DNDEBUG=1 -I../stb -g3
CFLAGS = -g3 -DDEBUG=1
#LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
LDFLAGS = -lglfw -lvulkan -ldl
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

test: template
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/test build/*.o $(LDFLAGS)

clean:
	rm -r bin
	rm -r tutorial/bin
	rm -r build
