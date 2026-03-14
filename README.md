# Happy-Lines

A fast, multi-threaded lines-of-code (LOC) counter written in C. It counts only git-tracked files — excluding auto-generated code, `node_modules`, build artifacts, and anything in `.gitignore` — using POSIX threads to parallelize the work.

## Features

- **Git-aware counting** — only counts lines in files tracked by git, so untracked and gitignored paths are automatically excluded
- **Multi-threaded scanning** — distributes files across up to 10 threads for parallel line counting
- **Per-contributor breakdown** — optional `--contributors` flag shows lines added/removed per git author
- **Folder exclusion** — interactively specify additional tracked folders to ignore (e.g. `vendor`, `dist`)
- **Configurable thread count** — control parallelism via `--threads=N`
- **Cross-platform** — platform abstraction layer supports Linux, macOS, and Windows (MSVC & MinGW)

## Quick Start

If you just want to get up and running as fast as possible:

```bash
git clone https://github.com/Pranav910/happy-lines.git
cd happy-lines
cmake -B build
cmake --build build
cd /path/to/your/project    # must be a git repository
/path/to/happy-lines/build/happy-lines --threads=4
```

## Prerequisites

Before building, make sure you have the following installed:

| Tool | Minimum Version | Check with |
|------|-----------------|------------|
| C compiler | Any (`gcc`, `clang`, or MSVC) | `gcc --version` or `clang --version` |
| CMake | 3.10+ | `cmake --version` |
| Git | Any recent version | `git --version` |

### Installing prerequisites

**macOS (Homebrew):**

```bash
xcode-select --install   # installs clang & git
brew install cmake
```

**Ubuntu / Debian:**

```bash
sudo apt update
sudo apt install build-essential cmake git
```

**Windows:**

Install [Visual Studio](https://visualstudio.microsoft.com/) (includes MSVC) or [MinGW-w64](https://www.mingw-w64.org/), then install [CMake](https://cmake.org/download/) and [Git for Windows](https://gitforwindows.org/).

## Building from Source

**1. Clone the repository:**

```bash
git clone https://github.com/Pranav910/happy-lines.git
cd happy-lines
```

**2. Configure and build:**

```bash
cmake -B build
cmake --build build
```

On a successful build you will see:

```
[100%] Built target happy-lines
```

The binary is at `build/happy-lines`.

**3. (Optional) Install globally:**

This lets you run `happy-lines` from any directory without specifying the full path.

```bash
sudo cmake --install build
```

Or copy manually:

```bash
# macOS / Linux
sudo cp build/happy-lines /usr/local/bin/

# Windows (run as Administrator)
copy build\Release\happy-lines.exe C:\Windows\System32\
```

To verify the installation:

```bash
happy-lines --help
```

## Usage

> **Important:** `happy-lines` must be run from inside a git repository. If you run it outside a git repo, it will exit immediately with an error.

### Basic LOC count

```bash
cd /path/to/your/git/repo
happy-lines --threads=4
```

### With per-contributor breakdown

```bash
happy-lines --threads=4 --contributors
```

### Show help

```bash
happy-lines --help
```

**Output:**

```
Usage: happy-lines [options]

Options:
  --threads=N       Number of threads for parallel LOC counting (default: 1, max: 10)
  --contributors    Show lines of code per git contributor
  --help            Show this help message
```

### Full example session

```
$ cd ~/projects/my-app
$ happy-lines --threads=4 --contributors

Threads: 4
Enter the folders to ignore (exit to stop):
vendor
dist
exit
Total happy lines count: 48523
Time taken: 0.012000 seconds

Analyzing git contributions...

--------------------------------------------------------------
 Contributor                        Added   Removed       Net
--------------------------------------------------------------
 Jane Smith                          1234       567       667
 John Doe                             890       123       767
--------------------------------------------------------------
 TOTAL                               2124       690      1434
--------------------------------------------------------------
```

### Running outside a git repository

```
$ cd /tmp
$ happy-lines --threads=4

Error: not inside a git repository.
happy-lines must be run from within a git repository.
```

### Ignoring folders

When the tool starts, it asks which folders to skip. Type one folder name per line, then `exit` to begin counting. These are matched against the beginning of each tracked file's path:

- Typing `vendor` ignores all files under `vendor/`
- Typing `docs` ignores all files under `docs/`
- Typing `exit` with no folders listed counts everything

Files already excluded by `.gitignore` are never counted regardless of this prompt.

## CLI Reference

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--threads=N` | integer | `1` | Number of threads for parallel file reading (max: 10) |
| `--contributors` | boolean | off | Show a per-author breakdown of lines added, removed, and net |
| `--help` | boolean | — | Print usage information and exit |

## How It Works

1. Parses CLI arguments (`--threads=N`, `--contributors`, `--help`).
2. Verifies the current directory is inside a git repository; exits with an error if not.
3. Changes to the repository root so all file paths are consistent.
4. Prompts the user for additional folders to exclude.
5. Runs `git ls-files` to collect all tracked files (anything in `.gitignore` is automatically excluded).
6. Filters out files in user-specified ignore folders.
7. Splits the file list evenly across the requested number of threads.
8. Each thread counts newlines in its assigned files.
9. Results from all threads are summed and printed alongside elapsed time.
10. If `--contributors` is enabled, queries `git log --numstat` per author and displays a sorted table.

## Project Structure

```
happy-lines/
├── CMakeLists.txt          # Build configuration (CMake)
├── include/
│   ├── platform.h          # Platform detection, compatibility macros, thread & dirent abstraction
│   ├── argparser.h         # CLI argument parser declarations
│   ├── counter.h           # LOC counter declarations
│   └── contributor.h       # Contributor analysis declarations
├── src/
│   ├── main.c              # Entry point — argument parsing, git validation, orchestration
│   ├── platform.c          # Platform abstraction layer implementation
│   ├── argparser.c         # CLI argument parser implementation
│   ├── counter.c           # Threaded LOC counting via git ls-files
│   └── contributor.c       # Per-contributor stats via git log
├── LICENSE                  # MIT License
└── README.md
```

## Troubleshooting

**`cmake: command not found`**
CMake is not installed. See the [Prerequisites](#prerequisites) section for installation instructions.

**`Error: not inside a git repository`**
You are running `happy-lines` from a directory that is not part of a git repo. `cd` into a git repository first.

**`No tracked files found`**
The repository has no files tracked by git. Make sure you have committed at least one file (`git add . && git commit -m "initial"`).

**Old version runs after rebuilding**
If you previously installed `happy-lines` globally, the shell may still find the old binary. Reinstall with `sudo cmake --install build` or `sudo cp build/happy-lines /usr/local/bin/` to update it.

**Build warnings on Windows**
MSVC may emit deprecation warnings for POSIX functions. These are suppressed by default via `#pragma warning(disable : 4996)` in `platform.h`.

## Uninstalling

If you installed globally and want to remove it:

```bash
# macOS / Linux
sudo rm /usr/local/bin/happy-lines

# Windows (run as Administrator)
del C:\Windows\System32\happy-lines.exe
```

To remove the build artifacts:

```bash
rm -rf build
```

## License

This project is licensed under the [MIT License](LICENSE).

## Contributing

Contributions are always welcome! Feel free to open issues or submit pull requests.
