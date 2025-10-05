# Development Guide

This section contains guidelines and resources for developers contributing to the Genius Gateway project.

## Code Standards

- **[Commenting Style Guide](commenting-style.md)** - Modern commenting standards for C/C++ code
- **Build System** - Information about PlatformIO and build configuration
- **Testing** - Unit testing guidelines and test structure

## Getting Started

1. **Prerequisites**: PlatformIO IDE/CLI, ESP-IDF toolchain
2. **Clone Repository**: `git clone https://github.com/hmbacher/genius-gateway.git`
3. **Build Firmware**: `pio run -e esp32-s3-devkitc-1`
4. **Development Workflow**: Use PlatformIO tasks for building and uploading

## Project Structure

```
├── src/                    # Main firmware source code
├── lib/framework/          # Custom ESP-IDF components
├── interface/              # SvelteKit web interface
├── scripts/                # Build automation scripts
├── docs/                   # Documentation source
└── tests/                  # Unit tests and test data
```

## Contributing

When contributing code to this project:

1. Follow the **[Commenting Style Guide](commenting-style.md)** for all C/C++ code
2. Ensure your code builds without warnings
3. Test your changes on actual hardware when possible
4. Update documentation for any public API changes

## IDE Setup

Recommended development environment:
- **PlatformIO IDE** (VS Code extension) or **CLion** with PlatformIO plugin
- Enable IntelliSense/code completion for optimal commenting integration
- Configure clang-format for consistent code formatting