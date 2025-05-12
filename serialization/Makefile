CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Wno-unused-function
LDFLAGS =
SRC = server.cpp buffer.cpp hashtable.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
