# File System Simulator with Directory Tree

A command-line file system simulator developed in **C++17**, designed to demonstrate fundamental operating-system concepts through an interactive filesystem environment.

The project implements an inode-based architecture, hierarchical directory management, hard and symbolic links, simulated users and permissions, file descriptors, persistent storage, and disk-block allocation.

The simulator provides a shell-like interface through which users can create, navigate, modify, inspect, and manage filesystem objects.

---

## Features

### 1. Directory and File Management
- Create and remove files and directories.
- Support absolute and relative path navigation.
- Display directory contents and hierarchical directory trees.
- Read and write file contents.
- Copy, move, and rename files.
- Support recursive directory deletion.
- Maintain special directory entries `.` and `..`.

### 2. Inode Management and Linking
- Assign unique inode IDs to filesystem objects.
- Maintain directory entries mapping filenames to inode IDs.
- Support hard links with shared inode data and link counts.
- Support symbolic links with stored target paths.
- Maintain inode metadata, including file type, ownership, permissions, and size.

### 3. Filesystem Persistence
- Save filesystem state to `filesystem_data.txt`.
- Restore saved filesystem data between application sessions.
- Validate filesystem structures during loading.
- Reconstruct simulated disk-block allocation after restoration.
- Support migration from older saved filesystem formats.

### 4. User and Permission Management
- Create and manage simulated users.
- Switch between simulated user accounts.
- Maintain owner UID and group GID metadata.
- Support permission modification using `chmod`.
- Support ownership modification using `chown`.
- Enforce basic access permissions on supported filesystem operations.

### 5. File Descriptor Management
- Open files in read, write, and read/write modes.
- Allocate file descriptor numbers starting from 3.
- Maintain independent file offsets.
- Support sequential reading.
- Support writing through open file descriptors.
- Close file descriptors after use.

### 6. Simulated Disk Management
- Simulated filesystem capacity: **1 MiB**.
- Block size: **512 bytes**.
- Total simulated blocks: **2,048**.
- First-fit block allocation.
- Automatic block release when file storage is no longer required.
- Reconstruct block allocation after filesystem restoration.
- Inspect individual file block mappings.
- Validate disk allocation consistency.

### 7. Command-Line Interface
- Interactive shell-like command interface.
- Dynamic prompt displaying the current user and directory.
- Built-in command help menu.
- Session-based command history.
- Command-name TAB autocomplete.
- Automatic filesystem saving after commands and during exit.

---

## Application Preview

The following screenshots demonstrate the major functionalities of the simulator through actual terminal executions.

### 1. Application Startup

Demonstrates filesystem initialization, saved-data restoration, disk reconstruction, and the interactive command-line interface.

![Application Startup]([PICTURE PATH BE PASTED HERE])

### 2. Directory Management and Navigation

Demonstrates directory creation, file creation, navigation, directory listing, and hierarchical tree visualization.

![Directory Management]([PICTURE PATH BE PASTED HERE])

### 3. Inodes and Link Management

Demonstrates hard links, symbolic links, inode sharing, link counts, and file metadata inspection.

![Inodes and Links]([PICTURE PATH BE PASTED HERE])

### 4. Simulated Disk Allocation and Storage Monitoring

Demonstrates file block allocation, disk usage reporting, storage utilization, and disk integrity validation.

![Disk Allocation]([PICTURE PATH BE PASTED HERE])

### 5. Permissions and File Descriptor Operations

Demonstrates simulated user management, permission modification, inode metadata, file opening, sequential reading, and descriptor closing.

![Permissions and File Descriptors]([PICTURE PATH BE PASTED HERE])

---

## System Architecture

The simulator follows an inode-based filesystem architecture in which directory entries reference inode IDs rather than directly storing file contents.

### Architecture Diagram

```text
                USER / COMMAND INPUT
                         |
                         v
                COMMAND-LINE INTERFACE
                         |
                         v
                   COMMAND PARSER
                         |
                         v
              PATH RESOLUTION & VALIDATION
                         |
                         v
                 IN-MEMORY INODE MAP
                    /           \
                   /             \
                  v               v
          DIRECTORY ENTRIES   INODE METADATA
          (Name -> Inode ID)  (Type, Owner,
                              Permissions,
                              Links, Content)
                  \               /
                   \             /
                    v           v
                  SIMULATED DISK
                  (512-BYTE BLOCKS)
                         |
                         v
                  PERSISTENCE LAYER
                 filesystem_data.txt
```

### Core Components

| Component | Responsibility |
|---|---|
| Inode Map | Stores filesystem objects using unique inode IDs |
| Directory Entries | Maps filenames and directory names to inode IDs |
| Inode Metadata | Stores file type, content, ownership, permissions, and link count |
| Path Resolver | Resolves absolute and relative filesystem paths |
| File Descriptors | Tracks open files, access modes, and current offsets |
| Disk Block Manager | Handles simulated block allocation and release |
| Persistence Layer | Saves and restores filesystem state |

### Storage Architecture

The simulator maintains file content inside inode structures. The simulated disk-block layer mirrors this content in fixed-size 512-byte chunks.

Examples:

| File Size | Required Blocks |
|---|---:|
| 0 bytes | 0 |
| 11 bytes | 1 |
| 512 bytes | 1 |
| 572 bytes | 2 |
| 1024 bytes | 2 |
| 1200 bytes | 3 |

The inode content remains the source of truth, while simulated disk blocks represent its allocated storage.

---

## Technology Stack

| Technology | Purpose |
|---|---|
| C++17 | Core implementation |
| STL Map | Inode tables, users, and directory entries |
| STL Vector | Disk blocks, file mappings, and command history |
| STL Set | Disk consistency validation |
| File I/O | Filesystem persistence |
| Stringstream | Command parsing |
| Windows Console API (`_getch`) | Command-name TAB completion |

---

## Installation and Execution

### Requirements

- Windows operating system.
- C++17-compatible compiler.
- GCC through MSYS2 UCRT64 is a supported build environment.
- PowerShell or Command Prompt.
- Windows console support for `<conio.h>` and `_getch()`.

### Compilation

Open PowerShell in the project directory.

```powershell
g++ .\main.cpp -o .\filesystem.exe -std=c++17 -fno-lto
```

### Execution

```powershell
.\filesystem.exe
```

On the first launch, the simulator initializes a new filesystem.

On subsequent launches, it attempts to restore the previously saved filesystem state from `filesystem_data.txt`.

**Important:** Keep `filesystem_data.txt` in the program's working directory to preserve filesystem data between sessions.

---

## Command Reference

Type `help` inside the simulator to display the available commands.

### 1. Directory and File Operations

| Command | Description | Example |
|---|---|---|
| `mkdir <name>` | Create a directory | `mkdir Projects` |
| `touch <name>` | Create an empty file | `touch notes.txt` |
| `ls` | List directory entries | `ls` |
| `ls -l` | Detailed directory listing | `ls -l` |
| `tree` | Display directory hierarchy | `tree` |
| `cd <path>` | Change current directory | `cd Projects` |
| `cd /` | Return to root directory | `cd /` |
| `pwd` | Display current directory path | `pwd` |
| `cat <path>` | Display file contents | `cat notes.txt` |
| `write <file> <text>` | Write content to a file | `write notes.txt Hello World` |
| `cp <source> <destination>` | Copy a file | `cp notes.txt backup.txt` |
| `mv <source> <destination>` | Move or rename a file | `mv notes.txt old.txt` |
| `rm <file>` | Remove a file or symbolic link | `rm notes.txt` |
| `rm -r <directory>` | Recursively remove a directory | `rm -r Projects` |
| `rmdir <directory>` | Remove an empty directory | `rmdir EmptyFolder` |

### 2. Links and Search

| Command | Description | Example |
|---|---|---|
| `ln <source> <link>` | Create a hard link | `ln report.txt backup.txt` |
| `ln -s <source> <link>` | Create a symbolic link | `ln -s report.txt shortcut` |
| `find <name>` | Search from current directory | `find report.txt` |
| `find <path> <name>` | Search from specified directory | `find /Projects report.txt` |

### 3. Metadata and Storage Management

| Command | Description | Example |
|---|---|---|
| `stat <path>` | Display inode metadata | `stat report.txt` |
| `inodes` | List active inodes | `inodes` |
| `du [path]` | Display directory storage usage | `du /Projects` |
| `df` | Display filesystem capacity and usage | `df` |
| `disk` | Display simulated disk status | `disk` |
| `blocks <file>` | Display allocated blocks for a file | `blocks report.txt` |
| `checkdisk` | Validate disk allocation consistency | `checkdisk` |

### 4. User and Permission Management

| Command | Description | Example |
|---|---|---|
| `useradd <username>` | Create a simulated user | `useradd alice` |
| `su <username>` | Switch simulated user | `su alice` |
| `users` | List simulated users | `users` |
| `whoami` | Display current user | `whoami` |
| `chmod <mode> <path>` | Change file permissions | `chmod 644 report.txt` |
| `chown <user> <path>` | Change file ownership | `chown alice report.txt` |

### 5. File Descriptor Operations

| Command | Description | Example |
|---|---|---|
| `open <path> <mode>` | Open a file (`r`, `w`, or `rw`) | `open report.txt rw` |
| `read <fd> [count]` | Read from an open descriptor | `read 3 10` |
| `write <fd> <text>` | Write through an open descriptor | `write 3 Updated Content` |
| `close <fd>` | Close an open descriptor | `close 3` |

The `write` command accepts either a filename or a numeric file descriptor as its first argument.

### 6. Shell and Utility Commands

| Command | Description |
|---|---|
| `history` | Display commands entered during the current session |
| `help` | Display the command reference |
| `clear` | Clear the terminal screen |
| `exit` | Save filesystem data and close the simulator |

---

## Example Execution

The following sequence demonstrates basic filesystem operations.

```text
fs> mkdir Projects
fs> cd Projects
fs> mkdir Cpp
fs> touch README.txt
fs> write README.txt File System Simulator Project
fs> ls
fs> tree
fs> pwd
fs> exit
```

### File Linking Example

```text
fs> touch report.txt
fs> write report.txt Operating System Project
fs> ln report.txt backup.txt
fs> ln -s report.txt shortcut
fs> ls -l
fs> stat report.txt
```

### Disk Management Example

```text
fs> disk
fs> blocks report.txt
fs> df
fs> du
fs> checkdisk
```

### File Descriptor Example

```text
fs> open report.txt r
fs> read 3 5
fs> read 3 5
fs> close 3
```

---

## Validation and Testing

The simulator includes functionality to inspect filesystem state and validate simulated disk allocation.

Recommended validation scenarios:

1. Create directories and verify the directory hierarchy.
2. Create files and verify their contents.
3. Create hard links and verify shared inode IDs and link counts.
4. Create symbolic links and verify target resolution.
5. Copy files and verify independent inode and block allocation.
6. Move or rename files and verify directory entries.
7. Open files and verify sequential descriptor offsets.
8. Modify permissions and ownership metadata.
9. Inspect disk allocation using `disk`, `blocks`, and `df`.
10. Execute `checkdisk` after file creation, copying, and deletion.
11. Restart the simulator and verify filesystem restoration.

---

## Project Limitations

This project is an educational filesystem simulation and does not implement a real operating-system filesystem.

- It does not mount a physical filesystem or access actual disk sectors.
- File contents are maintained in inode structures; simulated disk blocks mirror them.
- User management is simulated and does not provide password authentication.
- Permission handling implements a simplified model rather than the complete Unix permission system.
- Certain advanced Unix semantics, including full sticky-bit behavior and all intermediate-directory traversal checks, are not implemented.
- File descriptor state is runtime state and is not persisted between application sessions.
- TAB completion is Windows-console-specific.
- The simulator is designed for educational demonstration, not production or concurrent filesystem access.

---

## Project Structure

```text
FileSystemSimulator/
│
├── main.cpp
│
├── filesystem_data.txt
│
├── README.md
│
└── screenshots/
    ├── 01-startup.png
    ├── 02-directory-tree.png
    ├── 03-inodes-and-links.png
    ├── 04-disk-allocation.png
    └── 05-permissions-descriptors.png
```

`filesystem_data.txt` is generated automatically at runtime.

---

## Academic Project Information

**Project Title:** File System Simulator with Directory Tree

**Implementation Language:** C++17

**Project Domain:** Operating Systems and Systems Programming

**Primary Objective:** To demonstrate fundamental filesystem concepts and operating-system file management mechanisms through an interactive command-line simulation.

**Core Concepts Implemented:**
- Inode-based filesystem architecture
- Directory hierarchy and path resolution
- Hard and symbolic links
- File metadata and ownership
- Simulated users and permissions
- File descriptors and file offsets
- Persistent filesystem storage
- Simulated disk-block allocation
- Disk integrity validation

**Simulated Storage Specifications:**

| Parameter | Value |
|---|---:|
| Filesystem Capacity | 1 MiB |
| Block Size | 512 bytes |
| Total Blocks | 2,048 |
| Allocation Strategy | First-Fit |

---

**File System Simulator with Directory Tree — An educational implementation of core filesystem and operating-system concepts using C++17.**