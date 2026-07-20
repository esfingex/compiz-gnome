# Project Constitution & Guidelines

## Code & Quality Standards
- C++ Standard: C++20 with clean memory management (std::unique_ptr, std::shared_ptr).
- Build System: CMake 3.20+.
- Linting & Formatting: clang-format for C++, eslint / prettier for GJS.
- Memory & Safety: Zero unhandled raw pointers; memory leaks checked via Valgrind/AddressSanitizer.

## Supply-Chain & Package Rules
- All new dependencies must be validated via rtk package checks before installation.
- No direct modification of system libraries outside project root.

## Extension & Shell Safety
- GNOME Extensions must handle extension disable/enable cleanly without crashing gnome-shell.
- Resource cleanup on extension disable is mandatory.
