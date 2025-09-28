# Development

Developer resources for extending and contributing to the ESP32 Genius Gateway project.

## Getting Started

The Genius Gateway is built using modern development tools and practices:

- **Backend**: C++ with Arduino framework and PlatformIO
- **Frontend**: Svelte with TypeScript and Vite
- **Build System**: Integrated PlatformIO and npm workflows
- **Hardware**: KiCad for PCB design and schematic capture

## Development Environment

### Prerequisites
- PlatformIO IDE or CLI
- Node.js and npm for frontend development
- KiCad for hardware modifications (optional)
- Git for version control

### Project Structure
```
genius-gateway/
├── src/                 # C++ backend source code
├── interface/           # Svelte frontend application
├── lib/                 # Arduino libraries and dependencies
├── docs/               # Documentation source (this site)
├── cae/                # Hardware design files
└── scripts/            # Build and utility scripts
```

## Development Topics

- **[Build System](build-system.md)** - Setting up the development environment
- **[Code Style](code-style.md)** - Coding standards and guidelines
- **[Contributing](contributing.md)** - How to contribute to the project

## Architecture

The gateway uses a modular architecture with well-defined interfaces:

- **Service Layer** - Backend services for device management, MQTT, etc.
- **API Layer** - REST and WebSocket endpoints  
- **Frontend** - Responsive web application
- **Hardware Abstraction** - CC1101 radio and ESP32 interfaces

## Key Components

**C++ Services**
- `GeniusGateway` - Main service coordinator
- `GatewayDevicesService` - Device management
- `CC1101Controller` - Radio hardware interface
- `WSLogger` - WebSocket packet logging

**Frontend Components**
- Device management interface
- Real-time packet visualizer
- Settings and configuration panels
- Responsive design framework

## Contributing

We welcome contributions! Please see the [Contributing Guide](contributing.md) for:

- Code contribution guidelines
- Issue reporting procedures
- Feature request process
- Development workflow