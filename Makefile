CFLAGS = -std=c++17 -O2
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

all: template triangle

.PHONY: test clean

template: main.ccp
	g++ $(CFLAGS) -o vulkan main.cpp $(LDFLAGS)

triangle: triangle.cpp
	g++ $(CFLAGS) -o vulkan $^ $(LDFLAGS)

test: all
	./vulkan

clean:
	rm -f vulkan
