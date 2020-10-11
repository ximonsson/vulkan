CFLAGS = -std=c++17 -O2 -DNDEBUG=1
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

all: template triangle

.PHONY: test clean

template: main.cpp
	g++ $(CFLAGS) -o $@ $^ $(LDFLAGS)

triangle: triangle.cpp
	g++ $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: all
	./vulkan

clean:
	rm -f vulkan
