CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
TARGET = monitor
SRCS = terminal.cpp render_buffer.cpp cpu.cpp memory.cpp process.cpp app.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
