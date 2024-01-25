PORT := /dev/ttyACM0 # might be /dev/ttyUSB0

.PHONY: format
format:
	clang-format -i {main,tools}/*.[ch]
	cmake-format -i CMakeLists.txt main/CMakeLists.txt
	black tools/*.py
	nixfmt *.nix

build/inband-printer: tools/inband-printer.c tools/lib.h
	mkdir -p build
	$(CC) $< -o $@

build/print-text: tools/print-text.c main/clock.c main/text.c main/fonts.gen.c main/life.c tools/lib.h
	mkdir -p build
	$(CC) $(filter %.c,$^) -o $@

.PHONY: run-inband-printer
run-inband-printer: build/inband-printer
	picocom -X --noreset -b 460800 /dev/ttyUSB0
	build/inband-printer /dev/ttyUSB0
