CC       = gcc
CXX      = g++
DAISY    = ../dependencies/DaisySP/Source

CFLAGS   = -Iinclude -I$(DAISY) -I$(DAISY)/Utility -I$(DAISY)/Control \
           -I$(DAISY)/Effects -Wall -O2

CXXFLAGS = $(CFLAGS) -std=c++14

C_SRC    = $(filter-out src/chorus.c src/pitch_shifter.c src/overdrive.c, \
             $(wildcard src/*.c))

CPP_SRC  = src/chorus.cpp src/pitch_shifter.cpp src/overdrive.cpp \
           src/reverb.cpp \
           $(DAISY)/Control/phasor.cpp \
           $(DAISY)/Effects/overdrive.cpp

C_OBJ    = $(C_SRC:.c=.o)
CPP_OBJ  = $(CPP_SRC:.cpp=.o)

OUT      = audio_engine.exe

all: $(C_OBJ) $(CPP_OBJ)
	$(CXX) -o $(OUT) $^ -lm -lportaudio -lws2_32

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	del /Q $(OUT) src\*.o 2>NUL || true