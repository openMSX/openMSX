all: build/tsx2wav

CXXFLAGS += -I../../src/utils -I../../src/cassette -Wall -Wextra -O3 -std=c++23

build/%.o:%.cc Makefile
	mkdir -p build
	g++ -c $(CXXFLAGS) $< -o $@

build/TsxParser.o: ../../src/cassette/TsxParser.cc
	mkdir -p build
	g++ -c $(CXXFLAGS) $< -o $@

%: %.o build/TsxParser.o Makefile
	g++ $(filter %.o,$^) -o $@

clean:
	rm -rf build
