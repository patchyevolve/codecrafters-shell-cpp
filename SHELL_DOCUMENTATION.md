# Custom Shell Implementation Documentation

## Overview
This is a comprehensive Unix shell implementation in C++ that provides command execution, piping, redirection, history management, and tab completion features.

## Core Architecture

### Headers and Dependencies
```cpp
#include <iostream>      // Standard I/O operations
#include <fstream>       // File stream operations
#include <string>        // String manipulation
#include <vector>        // Dynamic arrays
#include <cstdlib>       // Environment variables (getenv)
#include <unistd.h>      // Unix system calls (access, fork, etc.)
#include <sys/wait.h>    // Process waiting functions
#include <filesystem>    // Modern C++ filesystem operations
#include <fcntl.h>       // File control operations
#include <readline/readline.h>  // GNU readline for input
#include <readline/history.h>   // Command history support
```

## Global Variables and Configuration

### History Management
```cpp
std::vector<std::string> history;           // Stores command history
const size_t HISTORY_LIMIT = 1000;         // Maximum history entries
std::string HISTFILE;                       // History file path
size_t session_start_index = 0;            // Track session start for appending
```

### Completion System
```cpp
static std::vector<std::string> completion_pool;  // Available completions
static std::vector<std::string> path_exec_cache;  // Cached executable names
static std::string cached_path_env;               // Cached PATH environment
static bool path_cache_built = false;            // Cache status flag
```

## History Management System

### 1. History File Resolution
```cpp
std::string get_histfile()
```
**Purpose**: Determines the history file location
**Logic**:
- Checks `HISTFILE` environment variable first
- Falls back to `$HOME/.my_shell_history`
- Default fallback: `.my_shell_history`

### 2. Reading History on Startup
```cpp
size_t history_read_file(const std::string& path)
```
**Features**:
- Reads existing history from file
- Enforces history limit (removes old entries)
- Returns number of entries loaded
- Handles file access errors gracefully

### 3. Writing History (Complete Overwrite)
```cpp
bool history_write_file(const std::string& path)
```
**Usage**: Saves entire history to file, overwriting existing content

### 4. Appending New Commands
```cpp
bool history_append_file(const std::string& path, size_t from_index)
```
**Efficiency**: Only appends new commands since session start#
# Built-in Commands System

### Command Detection
```cpp
std::vector<std::string> builtins = {"exit", "echo", "type", "pwd", "cd", "history"};
bool is_Builtin(std::string command)
```
**Purpose**: Identifies if a command should be handled internally vs external execution

### Built-in Command Implementations

#### 1. Exit Command
```cpp
if(cmd == "exit") return 0;
```
**Behavior**: Terminates shell and saves history

#### 2. PWD (Print Working Directory)
```cpp
fs::path currentPath = fs::current_path();
std::cout << currentPath.string() << std::endl;
```
**Implementation**: Uses C++17 filesystem library for cross-platform compatibility

#### 3. Echo Command
```cpp
for(size_t i = 1; i < argv.size(); ++i) {
    std::cout << argv[i];
    if(i + 1 < argv.size()) std::cout << " ";
}
```
**Features**: Prints arguments separated by spaces, adds newline at end

#### 4. CD (Change Directory)
**Complex Logic Handles**:
- `cd` (no args) → go to HOME
- `cd -` → go to previous directory (OLDPWD)
- `cd ~` → home directory expansion
- `cd ~/path` → home-relative paths
- Updates PWD and OLDPWD environment variables

#### 5. Type Command
**Functionality**:
- Checks if command is builtin
- Searches PATH directories for executables
- Reports full path of found commands
- Handles "command not found" cases

#### 6. History Command
**Advanced Features**:
- `history` → show all history
- `history N` → show last N commands
- `history -c` → clear history
- `history -w file` → write history to file
- `history -r file` → read history from file
- `history -a file` → append new commands to file

## Tab Completion System

### Path Executable Caching
```cpp
static void rebuild_path_exec_cache()
```
**Optimization Strategy**:
- Caches all executable files in PATH directories
- Only rebuilds when PATH environment changes
- Filters out non-executable files using `access()`
- Removes duplicates across directories

### Completion Generator
```cpp
static char* completion_generator(const char* text, int state)
```
**Readline Integration**:
- Called by readline library for each completion attempt
- Uses static index to iterate through matches
- Returns dynamically allocated strings (readline frees them)

### Smart Completion Logic
```cpp
static char** shell_completion(const char* text, int start, int end)
```
**Context-Aware Completion**:
- Only completes commands at start of line or after whitespace
- Combines builtins and PATH executables
- Sorts completions alphabetically
- Returns NULL for non-command positions (disables file completion)

## Advanced Tokenization System

### Quote Handling
```cpp
std::vector<std::string> tokenizer(std::string cmd)
```
**Sophisticated Parsing**:
- **Single quotes**: Preserve everything literally (no escaping)
- **Double quotes**: Allow escaping of `"` and `\`
- **Backslash escaping**: Outside quotes, escapes any character
- **Unclosed quote detection**: Returns empty vector with error message### Sp
ecial Character Recognition
**Pipe Detection**: `|` → Pipeline separator
**Redirection Operators**:
- `>` → Redirect stdout (truncate)
- `>>` → Redirect stdout (append)
- `<` → Redirect stdin
- `1>`, `1>>` → Explicit stdout redirection
- `2>`, `2>>` → Redirect stderr

**Whitespace Handling**: Properly separates tokens while preserving quoted content

## Redirection System

### Redirection Structure
```cpp
struct Redirection {
    int fd;                    // File descriptor (0=stdin, 1=stdout, 2=stderr)
    std::string filename;      // Target file
    enum { TRUNC, APPEND, READ } mode;  // Operation type
};
```

### Parsing Redirections
```cpp
std::pair<std::vector<std::string>, std::vector<Redirection>> RD_tokens(tokens)
```
**Separation Logic**:
- Extracts redirection operators and filenames from token stream
- Returns clean argument list + redirection list
- Handles syntax errors (missing filenames)

### Applying Redirections
```cpp
bool RD_apply(const std::vector<Redirection>& redirs, bool in_child)
```
**File Operations**:
- Opens files with appropriate modes (O_RDONLY, O_WRONLY|O_CREAT|O_TRUNC, etc.)
- Uses `dup2()` to redirect file descriptors
- Sets file permissions to 0644 (rw-r--r--)
- Handles errors differently for parent vs child processes

### File Descriptor Management
```cpp
struct FDSave { int in, out, err; };
FDSave save_FD();
void restorFD(const FDSave& s);
```
**Purpose**: Allows temporary redirection in parent process for builtins

## Pipeline Implementation

### Command Structure
```cpp
struct command {
    std::vector<std::string> argv;     // Command arguments
    std::vector<Redirection> redirs;   // Redirections for this command
    bool is_builtin = false;           // Optimization flag
};
```

### Pipeline Parsing
```cpp
std::vector<command> parse_pipeline(const std::vector<std::string>& tokens)
```
**Process**:
1. Split tokens by pipe (`|`) operators
2. Parse each segment for redirections
3. Validate no empty commands
4. Mark builtin commands for optimization

### Pipeline Execution
```cpp
int execute_pipeline(const std::vector<command>& cmds)
```
**Complex Process Management**:

#### Pipe Creation
```cpp
std::vector<std::array<int, 2>> pipes;
for(int i = 0; i < n-1; i++) {
    pipe(pipes[i].data());  // Create pipe for each connection
}
```

#### Process Forking and Connection
```cpp
for(int i = 0; i < n; i++) {
    pid_t pid = fork();
    if(pid == 0) {  // Child process
        // Connect input from previous pipe
        if(i > 0) dup2(pipes[i-1][0], 0);
        
        // Connect output to next pipe  
        if(i < n-1) dup2(pipes[i][1], 1);
        
        // Close all pipe file descriptors
        // Apply redirections
        // Execute command
    }
}
```

#### Resource Cleanup
- Parent closes all pipe file descriptors
- Waits for all child processes
- Returns exit status of last command in pipeline

## History Expansion System

### History Reference Parsing
```cpp
bool expand_history(std::string& cmd, const std::vector<std::string>& history)
```
**Supported Formats**:
- `!!` → Previous command
- `!n` → Command number n (1-based indexing)
- `!-n` → n commands ago
- Only works for single-word commands (no spaces)

**Error Handling**: Returns false for invalid references, prints error messages

## Main Shell Loop

### Readline Integration
```cpp
char* line = readline("$ ");
```
**Features**:
- Provides command-line editing (arrow keys, etc.)
- Integrates with history system
- Supports tab completion
- Handles Ctrl+D (EOF) gracefully

### Command Processing Pipeline
1. **Input Validation**: Skip blank lines
2. **History Expansion**: Process `!` references  
3. **Tokenization**: Parse quotes, operators, arguments
4. **History Storage**: Add to history (with exceptions)
5. **Pipeline Detection**: Check for `|` operators
6. **Execution**: Route to pipeline or single command handler

### Single Command Execution
**Builtin Handling**:
- Save file descriptors
- Apply redirections
- Execute builtin
- Restore file descriptors

**External Command Handling**:
- Fork child process
- Apply redirections in child
- Execute with `execvp()`
- Wait for completion in parent

## Key System Calls Used

### Process Management
- `fork()` → Create child processes
- `execvp()` → Replace process image with new program
- `waitpid()` → Wait for child process completion
- `_exit()` → Terminate child process (doesn't call destructors)

### File Operations  
- `open()` → Open files with specific modes
- `dup2()` → Duplicate file descriptors for redirection
- `close()` → Close file descriptors
- `access()` → Check file permissions and existence

### Environment
- `getenv()` → Read environment variables
- `setenv()` → Set environment variables (PWD, OLDPWD)
- `chdir()` → Change current directory

## Error Handling Strategies

### Graceful Degradation
- Missing history file → Continue without history
- Invalid PATH → Continue with builtins only
- Command not found → Print error, continue shell
- Syntax errors → Print error, continue shell

### Child Process Error Handling
- Use `_exit()` instead of `return` to avoid cleanup issues
- Print errors before exiting
- Use standard exit codes (127 for command not found)

### Resource Management
- Always close file descriptors after use
- Restore original file descriptors for builtins
- Clean up dynamically allocated memory from readline

This shell implementation demonstrates advanced Unix programming concepts including process management, inter-process communication, file I/O, and signal handling, all wrapped in a user-friendly interface with modern C++ features.