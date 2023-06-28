PORT := /dev/ttyACM0 # might be /dev/ttyUSB0

.PHONY: format
format:
	clang-format -i {main,tools}/*.[ch]
	cmake-format -i CMakeLists.txt main/CMakeLists.txt
	nixfmt *.nix

build/inband-printer: tools/inband-printer.c
	mkdir -p build
	$(CC) $< -o $@

.PHONY: run-inband-printer
run-inband-printer: build/inband-printer
	picocom -X --noreset -b 460800 /dev/ttyUSB0
	build/inband-printer /dev/ttyUSB0
