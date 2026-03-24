CC       = gcc
CXX      = g++
DAISY = ../dependencies/DaisySP/Source

CXXFLAGS = -Iinclude -I$(DAISY) -I$(DAISY)/Utility -I$(DAISY)/Control \
           -I$(DAISY)/Effects -Wall -O2 -std=c++14

C_SRC   = $(filter-out src/chorus.c src/pitch_shifter.c src/overdrive.c, $(wildcard src/*.c))
CPP_SRC = src/chorus.cpp src/pitch_shifter.cpp src/overdrive.cpp \
          $(DAISY)/Control/phasor.cpp \
          $(DAISY)/Effects/overdrive.cpp \
          $(DAISY)/Effects/reverbsc.cpp

C_OBJ   = $(C_SRC:.c=.o)
CPP_OBJ = $(CPP_SRC:.cpp=.o)

OUT = audio_engine

all: $(C_OBJ) $(CPP_OBJ)
	$(CXX) -o $(OUT) $^ -lm -lasound -lsndfile

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OUT) src/*.o $(DAISY)/Control/*.o $(DAISY)/Effects/*.o