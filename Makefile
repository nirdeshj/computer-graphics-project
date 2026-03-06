CXX = C:/mingwc/bin/g++.exe
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fdiagnostics-color=always -g
TARGET = run.exe
SOURCES = src/main.cpp
LINK = -lbgi -lgdi32 -lcomdlg32 -luuid -loleaut32 -lole32

# Default target
all: $(TARGET)

$(TARGET): $(SOURCES)
	@echo Compiling with graphics.h support...
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LINK)
	@echo Compilation complete. Output: $(TARGET)

# Clean target
clean:
	@echo Cleaning build files...
	@if exist $(TARGET) del $(TARGET)
	@echo Clean complete.

# Run target
run: $(TARGET)
	@echo Running program...
	./$(TARGET)

# Phony targets
.PHONY: all clean run