CXX = g++
CXXFLAGS = -std=c++11 -Wall -Ivendor/googletest -O3 -DNDEBUG
LDFLAGS =

trax: main.o trax.o
	$(CXX) $^ $(LDFLAGS) -o $@

test: trax_test
	./trax_test

trax_test: trax_test.o trax.o gtest-all.o
	$(CXX) $^ $(LDFLAGS) -o $@

gtest-all.o: vendor/googletest/gtest/gtest-all.cc
	$(CXX) -c $^ $(CXXFLAGS) -o $@

clean:
	rm -f *.o trax trax_test

.PHONY: all test clean
