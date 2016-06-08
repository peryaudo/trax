CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wno-unused-const-variable -Ivendor/googletest -Ivendor/gflags -O3 -DNDEBUG
LDFLAGS =

trax: main.o trax.o gflags.o gflags_completions.o gflags_reporting.o
	$(CXX) $^ $(LDFLAGS) -o $@

test: trax_test
	./trax_test

trax_test: trax_test.o trax.o gtest-all.o
	$(CXX) $^ $(LDFLAGS) -o $@

trax_test.o: trax_test.cc trax.h

trax.o: trax.cc trax.h

main.o: main.cc trax.h

gtest-all.o: vendor/googletest/gtest/gtest-all.cc
	$(CXX) -c $^ $(CXXFLAGS) -o $@

gflags.o: vendor/gflags/gflags.cc
	$(CXX) -c $^ -o $@

gflags_completions.o: vendor/gflags/gflags_completions.cc
	$(CXX) -c $^ -o $@

gflags_reporting.o: vendor/gflags/gflags_reporting.cc
	$(CXX) -c $^ -o $@

clean:
	rm -f *.o trax trax_test

.PHONY: all test clean
