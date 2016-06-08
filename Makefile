CXX = g++
CXXFLAGS = -std=c++11 -Wall -Ivendor/googletest -O3 -DNDEBUG
LDFLAGS =

trax: main.o trax.o
	$(CXX) $^ $(LDFLAGS) -o $@

trax_test: trax_test.o trax.o
	$(CXX) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o trax trax_test

.PHONY: test clean
