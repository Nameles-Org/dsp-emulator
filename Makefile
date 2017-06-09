CC=gcc
CXX=g++
RM=rm -f
CXXFLAGS=-Wall -std=c++11 -I./include
LDFLAGS=-L./lib/
LDLIBS=-lm -lrt -lgflags -lpqxx -lboost_system -lpthread -lboost_thread -lzmqpp -lpq -lzmq
SRCDIR=./src/
SRC=dsp_latency_test.cpp
BIN=dsp_latency_test

all: $(BIN)

$(BIN): $(SRCDIR)$(SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(BIN) $(SRCDIR)$(SRC) $(LDLIBS) 

.PHONY: clean

clean:
	$(RM) $(BIN)
