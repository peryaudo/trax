CXX = g++
CXXFLAGS = -std=c++11 -Wall -O3 -Ivendor/googletest -DNDEBUG
LDFLAGS =

trax: trax.o main.o
	$(CXX) $^ $(LDFLAGS) -o $@

trax_test: trax_test.o
	$(CXX) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o trax trax_test

.PHONY: test clean
