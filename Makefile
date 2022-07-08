.PHONY: build
build:
	g++ main.cpp -o lc3-vm

.PHONY: 2048
2048: build
	./lc3-vm ./2048.obj
