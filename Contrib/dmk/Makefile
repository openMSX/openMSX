TARGETS = analyze-dmk combine-dmk der2dmk dsk2dmk empty-dmk svi2dmk svicpm2dmk

all: $(addprefix build/,$(TARGETS))

CXXFLAGS += -std=c++23 -Wall -Wextra -O3

build/%.o:%.cc Makefile
	mkdir -p build
	g++ -c $(CXXFLAGS) $< -o $@

%:%.o Makefile
	g++ $< -o $@

clean:
	rm -rf build
