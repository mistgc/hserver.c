compile: build
	ninja -j8 -C build

build: CMakeLists.txt
	cmake -B build -G Ninja

run: compile
	@./build/hserver

clean:
	rm -rf build

.PHONY: complie build run clean
