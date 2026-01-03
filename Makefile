# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -I./include
LDFLAGS = -pthread

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = .

# Target executable
TARGET = $(BIN_DIR)/proxy_server

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✅ Build successful! Executable: $(TARGET)"

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)
	@echo "🧹 Cleaned build artifacts"

# Clean everything including logs
distclean: clean
	rm -f logs/*.log
	@echo "🧹 Cleaned logs"

# Run the proxy server
run: $(TARGET)
	./$(TARGET)

# Run with custom config
run-config: $(TARGET)
	./$(TARGET) $(CONFIG)

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: clean all

# Release build with optimizations
release: CXXFLAGS += -O3 -DNDEBUG
release: clean all

# Install (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	@echo "✅ Installed to /usr/local/bin/"

# Uninstall
uninstall:
	rm -f /usr/local/bin/proxy_server
	@echo "🗑️  Uninstalled from /usr/local/bin/"

# Help
help:
	@echo "Available targets:"
	@echo "  make          - Build the project"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make run      - Build and run the server"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make release  - Build optimized release version"
	@echo "  make install  - Install to system"
	@echo "  make help     - Show this help"

.PHONY: all clean distclean run run-config debug release install uninstall help
