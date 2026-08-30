CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -Iinclude -Iinclude/core -Iinclude/collectors -Iinclude/ui
TARGET = monitor
SRCDIR = src

SRCS = $(wildcard $(SRCDIR)/*.cpp) \
       $(wildcard $(SRCDIR)/core/*.cpp) \
       $(wildcard $(SRCDIR)/collectors/*.cpp) \
       $(wildcard $(SRCDIR)/ui/*.cpp)

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	-cmd /c del /Q /F src\*.o src\core\*.o src\collectors\*.o src\ui\*.o $(TARGET).exe $(TARGET) 2>NUL
else
	rm -f src/*.o src/*/*.o $(TARGET)
endif

.PHONY: all clean
