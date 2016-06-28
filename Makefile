CXX = g++

# Release
CXXFLAGS = -std=c++11 -Wall -Wno-unused-const-variable -Wno-strict-aliasing -Wno-maybe-uninitialized -Wno-unused-variable -Ivendor/googletest -Ivendor/gflags -O3 -DNDEBUG
LDFLAGS = -O3 -lpthread

# Debug
# CXXFLAGS = -std=c++11 -Wall -Wno-unused-const-variable -Wno-tautological-constant-out-of-range-compare -Ivendor/googletest -Ivendor/gflags -O1 -g -fsanitize=address #-pg -DNDEBUG
# LDFLAGS = -O1 -fsanitize=address -fno-omit-frame-pointer -lpthread #-pg -DNDEBUG

trax: main.o trax.o search.o gflags.o gflags_completions.o gflags_reporting.o perft.o
	$(CXX) $^ $(LDFLAGS) -o $@

test: trax_test
	./trax_test

trax_test: trax_test.o trax.o search.o gtest-all.o gflags.o gflags_completions.o gflags_reporting.o perft.o
	$(CXX) $^ $(LDFLAGS) -o $@

trax_test.o: trax_test.cc trax.h timer.h

trax.o: trax.cc trax.h

main.o: main.cc trax.h search.h perft.h

search.o: search.cc search.h trax.h

perft.o: perft.cc perft.h trax.h

gtest-all.o: vendor/googletest/gtest/gtest-all.cc
	$(CXX) -std=c++03 -c $^ $(CXXFLAGS) -o $@

gflags.o: vendor/gflags/gflags.cc
	$(CXX) -std=c++03 -c $^ -o $@

gflags_completions.o: vendor/gflags/gflags_completions.cc
	$(CXX) -std=c++03 -c $^ -o $@

gflags_reporting.o: vendor/gflags/gflags_reporting.cc
	$(CXX) -std=c++03 -c $^ -o $@

clean:
	rm -f *.o trax trax_test

lint:
	cpplint *.cc *.h
	cloc *.cc *.h

.PHONY: all test clean lint
