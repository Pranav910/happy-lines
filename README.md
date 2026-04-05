# Happy-Lines

A fast, multi-threaded lines-of-code (LOC) counter written in C. It counts only git-tracked files — excluding auto-generated code, `node_modules`, build artifacts, and anything in `.gitignore` — using POSIX threads to parallelize the work.

## Features

- **Git-aware counting** — only counts lines in files tracked by git, so untracked and gitignored paths are automatically excluded
- **Force mode** — `--force` counts all files in the current directory (tracked and untracked); no git required
- **Dual worker-pool parallelism** — separate `--directory-workers` and `--file-workers` thread pools for directory traversal and file processing (inspired by SCC)
- **Per-contributor breakdown** — optional `--contributors` flag shows lines added/removed per git author
- **LOC by file extension** — optional `--by-extension` prints a table of line counts grouped by extension (git-tracked files only; not compatible with `--force`)
- **Folder exclusion** — interactively specify folders to ignore in both normal and force mode (e.g. `vendor`, `dist`)
- **Auto-tuned defaults** — worker counts default to the number of logical CPUs on the host
- **Cross-platform** — platform abstraction layer supports Linux, macOS, and Windows (MSVC & MinGW)

## Quick Start

If you just want to get up and running as fast as possible:

```bash
git clone https://github.com/Pranav910/happy-lines.git
cd happy-lines
cmake -B build
cmake --build build
cd /path/to/your/project    # must be a git repository
/path/to/happy-lines/build/happy-lines
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

> **Normal mode:** `happy-lines` must be run from inside a git repository. Use `--force` to count all files in the current directory without requiring git.

### Basic LOC count (git-tracked only)

```bash
cd /path/to/your/git/repo
happy-lines
```

### Custom worker counts

```bash
happy-lines --file-workers=8
happy-lines --directory-workers=4 --file-workers=8 --force
```

### Force mode (all files, no git required)

Count every file under the current directory, including untracked and gitignored files. You can still exclude folders at the prompt.

```bash
cd /path/to/any/directory
happy-lines --force
```

### With per-contributor breakdown

```bash
happy-lines --contributors
```

### LOC by file extension

After the usual folder-ignore prompt, aggregates lines by extension (the part of the filename after the last `.`). Extensions are sorted from most to least lines, then a total row is shown. This mode only applies when counting **git-tracked** files; it cannot be combined with `--force` (the program exits with an error if both flags are set).

```bash
happy-lines --by-extension
```

**Sample output** (table appears after counting, before the overall total and timing):

```
-------------------------------
 Extension               Lines
-------------------------------
 c                        1079
 md                        294
 h                         212
-------------------------------
 TOTAL                    1585
-------------------------------
```

### Show help

```bash
happy-lines --help
```

**Output:**

```
Usage: happy-lines [options]

Options:
  --directory-workers=N  Threads for parallel directory traversal (default: CPU count, max: 32)
  --file-workers=N      Threads for parallel file processing (default: CPU count, max: 32)
  --force               Count all files (tracked and untracked); no git required
  --contributors        Show lines of code per git contributor
  --by-extension        Show lines of code per file extension (git-tracked only; not with --force)
  --help                Show this help message
```

### Full example session

```
$ cd ~/projects/my-app
$ happy-lines --file-workers=8 --contributors

File workers: 8
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

### Force mode example

```
$ cd /tmp/some-project
$ happy-lines --directory-workers=4 --file-workers=8 --force

Directory workers: 4
File workers: 8
Mode: force (all files, tracked + untracked)
Enter the folders to ignore (exit to stop):
node_modules
exit
Total happy lines count: 12345
Time taken: 0.050000 seconds
```

### Running outside a git repository

Without `--force`, the tool requires a git repository:

```
$ cd /tmp
$ happy-lines

Error: not inside a git repository.
happy-lines must be run from within a git repository.
Use --force to count all files in the current directory.
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
| `--directory-workers=N` | integer | CPU count | Threads for parallel directory traversal — used in `--force` mode (max: 32) |
| `--file-workers=N` | integer | CPU count | Threads for parallel file line counting (max: 32) |
| `--force` | boolean | off | Count all files (tracked + untracked); no git required |
| `--contributors` | boolean | off | Show a per-author breakdown of lines added, removed, and net |
| `--by-extension` | boolean | off | Show LOC grouped by file extension (git-tracked only; mutually exclusive with `--force`) |
| `--help` | boolean | — | Print usage information and exit |

## How It Works

1. Parses CLI arguments (`--directory-workers=N`, `--file-workers=N`, `--force`, `--contributors`, `--by-extension`, `--help`).
2. If both `--force` and `--by-extension` are set, exits with an error (extension breakdown is only implemented for git-tracked files).
3. If `--force` is not set: verifies the current directory is inside a git repository and changes to the repo root; exits with an error if not in a repo.
4. Prompts the user for folders to exclude (applies in both normal and force mode).
5. **Normal mode:** Runs `git ls-files` to collect tracked files, filters by ignore list, dispatches file workers to count lines in parallel. With `--by-extension`, each worker also accumulates per-extension counts; these are merged and printed as a sorted table before the overall total.
6. **Force mode (two-phase pipeline):**
   - *Phase 1 — Directory walking:* Enumerates top-level subdirectories, distributes them across directory-worker threads. Each worker recursively walks its assigned directories and collects discovered file paths into a per-thread list.
   - *Phase 2 — File processing:* All per-thread file lists are merged, then split across file-worker threads for parallel line counting.
7. Results from all workers are summed and printed alongside elapsed time.
8. If `--contributors` is enabled (and the directory is a git repo), queries `git log --numstat` per author and displays a sorted table.

## Project Structure

```
happy-lines/
├── CMakeLists.txt          # Build configuration (CMake)
├── include/
│   ├── platform.h          # Platform detection, compatibility macros, thread & dirent abstraction
│   ├── argparser.h         # CLI argument parser declarations
│   ├── counter.h           # LOC counter declarations
│   ├── hash_map.h          # Small string→int map for per-extension aggregation
│   └── contributor.h       # Contributor analysis declarations
├── src/
│   ├── main.c              # Entry point — argument parsing, git validation, orchestration
│   ├── platform.c          # Platform abstraction layer implementation
│   ├── argparser.c         # CLI argument parser implementation
│   ├── counter.c           # Threaded LOC counting via git ls-files
│   ├── hash_map.c          # Hash map implementation (used by extension breakdown)
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
