CFLAGS = -std=c++17 -O2
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

all: main.cpp
	g++ $(CFLAGS) -o vulkan main.cpp $(LDFLAGS)

.PHONY: test clean

test: all
	./vulkan

clean:
	rm -f vulkan
