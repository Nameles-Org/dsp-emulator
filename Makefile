CC=gcc
CXX=g++
RM=rm -f
CXXFLAGS=-Wall -std=c++14 -I./include
LDFLAGS=-L./lib/
LDLIBS=-lm -lrt -lgflags -lpqxx -lboost_system -lpthread -lboost_thread -lzmqpp -lpq -lzmq
SRCDIR=./src/
BIN=dsp_latency_test
TESTSBIN=test_data_processing

all: $(BIN) $(TESTSBIN)

$(BIN): $(addprefix $(SRCDIR), $(addsuffix .cpp,$(BIN)))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TESTSBIN): $(addprefix $(SRCDIR), $(addsuffix .cpp,$(TESTSBIN)))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean

clean:
	$(RM) $(BIN) $(TESTSBIN)
