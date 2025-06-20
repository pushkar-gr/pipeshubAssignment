CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
SOURCES = order_management.cpp main.cpp
TARGET = order_management_system
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) order_responses.log

.PHONY: all clean
