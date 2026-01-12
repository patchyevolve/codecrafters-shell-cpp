# Custom Unix Shell Implementation

[![progress-banner](https://backend.codecrafters.io/progress/shell/ba39e9b8-b85c-4a24-8d20-ec71d54f91da)](https://app.codecrafters.io/users/codecrafters-bot?r=2qF)

A fully-featured POSIX-compliant shell implementation in C++ with advanced features including pipelines, redirection, history management, and tab completion.

## Features

### Core Shell Functionality
- **Command Execution**: Run external programs and built-in commands
- **Pipeline Support**: Chain commands with `|` operator
- **I/O Redirection**: Support for `>`, `>>`, `<`, `2>`, `2>>` operators
- **Built-in Commands**: `cd`, `pwd`, `echo`, `type`, `history`, `exit`
- **Tab Completion**: Intelligent command and path completion
- **Command History**: Persistent history with expansion (`!!`, `!n`, `!-n`)

### Advanced Features
- **Quote Handling**: Proper parsing of single and double quotes with escaping
- **History Management**: Configurable history file with append/write modes
- **Environment Integration**: HOME, PATH, PWD, OLDPWD variable handling
- **Error Handling**: Graceful error recovery and informative messages
- **Process Management**: Proper fork/exec/wait cycle with resource cleanup

## Technical Highlights
- Modern C++17 with filesystem library
- GNU Readline integration for enhanced user experience
- Efficient PATH executable caching system
- Robust tokenization with complex quote and escape handling
- Multi-process pipeline execution with proper file descriptor management

## Quick Start

### Prerequisites
- C++17 compatible compiler (GCC 7+ or Clang 5+)
- CMake 3.10+
- GNU Readline library
- Unix/Linux environment (WSL supported)

### Installation
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential cmake libreadline-dev

# Clone and build
git clone <repository-url>
cd codecrafters-shell-cpp
mkdir build && cd build
cmake ..
make
```

### Running the Shell
```bash
# From project root
./your_program.sh

# Or run directly
./src/main.exe
```

## Usage Examples

### Basic Commands
```bash
$ echo "Hello, World!"
Hello, World!

$ pwd
/home/user/project

$ cd /tmp
$ pwd
/tmp
```

### Pipelines and Redirection
```bash
$ ls -la | grep ".cpp" > cpp_files.txt
$ cat cpp_files.txt | wc -l
$ echo "Error message" 2> error.log
```

### History Features
```bash
$ history          # Show command history
$ !!              # Repeat last command
$ !5              # Execute command #5
$ !-2             # Execute command 2 steps back
```

### Tab Completion
- Press `Tab` to complete commands and paths
- Works with built-ins and PATH executables
- Intelligent context-aware completion

## Built-in Commands

| Command | Description | Examples |
|---------|-------------|----------|
| `cd [dir]` | Change directory | `cd`, `cd ~`, `cd -`, `cd /path` |
| `pwd` | Print working directory | `pwd` |
| `echo [args...]` | Print arguments | `echo hello world` |
| `type <cmd>` | Show command type/location | `type ls`, `type cd` |
| `history [n]` | Show command history | `history`, `history 10` |
| `history -c` | Clear history | `history -c` |
| `history -w <file>` | Write history to file | `history -w ~/.history` |
| `history -r <file>` | Read history from file | `history -r ~/.history` |
| `exit` | Exit shell | `exit` |

## Architecture Overview

### Core Components
- **Tokenizer**: Advanced parsing with quote handling and escape sequences
- **Command Parser**: Separates commands, arguments, and redirection operators
- **Pipeline Engine**: Multi-process execution with proper IPC
- **Built-in Handler**: Efficient built-in command execution
- **History System**: Persistent command history with file management
- **Completion Engine**: PATH-aware tab completion system

### Key Files
- `src/main.cpp` - Main shell implementation
- `SHELL_DOCUMENTATION.md` - Comprehensive technical documentation
- `CMakeLists.txt` - Build configuration
- `your_program.sh` - Execution script

## Configuration

### Environment Variables
- `HISTFILE` - Custom history file location (default: `~/.my_shell_history`)
- `HOME` - User home directory for `cd` and `~` expansion
- `PATH` - Executable search paths for command completion
- `PWD` - Current working directory (maintained by shell)
- `OLDPWD` - Previous directory for `cd -`

### History Configuration
```cpp
const size_t HISTORY_LIMIT = 1000;  // Maximum history entries
enum class histpersistence { WRITE, APPEND };  // History save mode
```

## Development

### Building from Source
```bash
# Debug build
mkdir debug && cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Release build  
mkdir release && cd release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Testing
```bash
# Run shell and test features
./src/main.exe

# Test specific functionality
echo 'ls | grep ".cpp"' | ./src/main.exe
```

## Contributing

This project was built as part of the [CodeCrafters Shell Challenge](https://app.codecrafters.io/courses/shell/overview). 

### Areas for Enhancement
- Signal handling (Ctrl+C, Ctrl+Z)
- Job control (background processes)
- Alias support
- Shell scripting features
- Additional built-in commands

## Documentation

For detailed technical documentation including implementation details, system calls used, and architectural decisions, see [SHELL_DOCUMENTATION.md](SHELL_DOCUMENTATION.md).

## License

This project is part of the CodeCrafters challenge. See CodeCrafters terms for usage rights.

---

**Note**: This shell requires a Unix-like environment. On Windows, use WSL (Windows Subsystem for Linux) for full functionality.
