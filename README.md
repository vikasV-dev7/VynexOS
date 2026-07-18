# VynexOS

## Project Overview
VynexOS is an experimental, modern operating system runtime architecture built entirely in C++23. Focused on clean abstractions, modular service design, and strict architectural integrity, VynexOS provides a robust foundation for building advanced hardware interaction frameworks. 

Currently, the project is configured as a hosted runtime (simulated within an existing OS process) rather than a bare-metal kernel, allowing for rapid iteration on the application and desktop frameworks using mock hardware abstractions.

## Vision of VynexOS
The vision of VynexOS is to deliver a beautifully designed, structurally immaculate, and strictly typed OS architecture where the line between kernel, driver, and application is mediated cleanly through event buses and strictly defined interfaces. We prioritize code quality, maintainability, and architectural discipline over rapid feature bloat.

## Architecture Overview
The system is divided into highly cohesive, loosely coupled sub-modules:
- **Core (`vynex_core`)**: The system foundation, encompassing the Logger, Service Manager, Configuration, IPC Framework, Plugin Manager, and the central Event Bus.
- **HAL (`vynex_hal`)**: Hardware Abstraction Layer. Defines strict interfaces for Compute, Audio, Input, and generic devices.
- **Desktop (`vynex_desktop`)**: The graphical substrate. Includes the Compositor, Window Manager, Widget Toolkit, and Notification Service.
- **Apps (`vynex_apps`)**: System applications like the Desktop Shell, Terminal, File Explorer, and Demo App that rely on `vynex_desktop`.
- **Bootstrap (`vynex_bootstrap`)**: The Composition Root that wires all dependencies via dependency injection and launches the main event loop.

## Current Status
**Architecture Version 0.5.0 (Stable)**
The core subsystem architectures have been defined, implemented with mock interfaces, and verified to compile and execute flawlessly under MSVC C++23. The Physical Driver Layer (v0.5.0) introduces hardware-accelerated rendering and physical input translation via SDL2, replacing previously mocked subsystems.

## Features Implemented
✓ Runtime Framework  
✓ Composition Root  
✓ Dependency Injection  
✓ Logger  
✓ Event Bus  
✓ IPC Framework  
✓ Plugin Manager  
✓ Window Manager  
✓ Compositor  
✓ Desktop Shell  
✓ Terminal  
✓ File Explorer  
✓ Notification Service  
✓ SDL2 Display Backend (Hardware Accelerated)  
✓ SDL2 Input Driver (Physical Keyboard & Mouse)  
✓ FrameClock (60 FPS Timing Synchronization)  

*(Note: The current runtime leverages physical hardware bindings for display and input via SDL2. Audio and compute implementations remain mocked simulating physical execution until future sprints.)*

## Build Instructions
1. Clone the repository.
2. Open a Visual Studio Developer Command Prompt.
3. Configure the project: `cmake -B build -G "Visual Studio 17 2022"`
4. Build the project: `cmake --build build --config Debug`
5. Run the executable: `.\build\bin\Debug\vynex_init.exe`

## Development Environment
- **Compiler**: MSVC 2022 (Version 17.10+)
- **Standard**: C++23
- **Build System**: CMake 3.25+
- **OS**: Windows (Targeting Windows 10 SDK)
- **Settings**: `/WX` (Warnings as Errors) is strictly enforced.

## Repository Structure
```
/
├── assets/         # Static assets, fonts, icons, etc.
├── docs/           # Documentation and architecture diagrams
├── include/        # Public API headers (vynexos/...)
├── scripts/        # Build and automation scripts
├── src/            # Source code (apps, bootstrap, core, desktop, hal)
├── tests/          # Unit testing framework
├── CMakeLists.txt  # Root CMake configuration
├── README.md       # Project documentation
├── LICENSE         # MIT License
└── .gitignore      # Ignored artifacts
```

## Roadmap
1. **v0.3.0**: Runtime Extension Framework (Dynamic Plugin ABI).
2. **v0.4.0**: Hardware Abstraction Layer completion.
3. **v0.5.0 (Current)**: Physical Graphical Compositor (SDL2/OpenGL).
4. **v0.6.0**: Hardware-Accelerated UI Compositing and Event Routing.

## Current Limitations
- Mock Audio Backend (No actual sound playback)
- Mock Compute Backend (Simulated GPU workloads)
- Mock File System (No physical disk mapping)
- No memory manager or bare-metal scheduler
- Runs as a hosted runtime rather than a standalone operating system

## Future Work
- Implementation of a Vynex UI layer.
- Hardware-accelerated UI compositing logic.
- Advancing the `WidgetToolkit` with standard UI components (buttons, sliders, text inputs).

## License
This project is licensed under the MIT License. See the `LICENSE` file for more details.

## Contributing
All contributions must adhere to the VynexOS architectural philosophy. Code must compile without warnings, utilize C++23 paradigms (`std::expected`, `[[nodiscard]]`, `std::span`), and rely on dependency injection rather than global singletons.
