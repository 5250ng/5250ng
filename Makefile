# Makefile for 5250ng project
# Based on build instructions from README.md

.PHONY: all build clean install-deps run help

# Detect OS
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Default target
all: build

# Build the project
build:
	@echo "Building project..."
	@cmake -S . -B build
	@cmake --build build
	@echo "Build complete!"

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	@rm -rf build
	@echo "Clean complete!"

# Install dependencies (Linux)
install-deps-linux:
	@echo "Installing dependencies for Linux..."
	@sudo apt install qt6-base-dev cmake g++

# Install dependencies (macOS)
install-deps-macos:
	@echo "Installing dependencies for macOS..."
	@brew install qt cmake

# Install dependencies (auto-detect OS)
install-deps:
ifeq ($(UNAME_S),Linux)
	@$(MAKE) install-deps-linux
else ifeq ($(UNAME_S),Darwin)
	@$(MAKE) install-deps-macos
else
	@echo "Please install dependencies manually for your OS"
	@echo "See README.md for instructions"
endif

# Run the application
run: build
ifeq ($(UNAME_S),Darwin)
	@./build/tn5250.app/Contents/MacOS/tn5250
else
	@./build/tn5250
endif

# Windows build (PowerShell)
build-windows:
	@echo "Building for Windows..."
	@cmake -G "Visual Studio 17 2022" -S . -B build
	@cmake --build build --config Release
	@echo "Build complete!"

# Help target
help:
	@echo "5250ng Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Build the project (default)"
	@echo "  build            - Build the project using CMake"
	@echo "  clean            - Remove build directory"
	@echo "  install-deps     - Install dependencies (Linux/macOS)"
	@echo "  run              - Build and run the application"
	@echo "  build-windows    - Build for Windows (Visual Studio)"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make             - Build the project"
	@echo "  make clean       - Clean build artifacts"
	@echo "  make install-deps - Install dependencies"
	@echo "  make run         - Build and run"

