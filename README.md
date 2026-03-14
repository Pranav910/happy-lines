# Happy-Lines

A fast, multi-threaded lines-of-code (LOC) counter written in C. It recursively walks directories, counts every line across all files, and reports the total — using POSIX threads to parallelize the work.

## Features

- **Multi-threaded scanning** — distributes directories across up to 10 threads for parallel file reading
- **Folder exclusion** — interactively specify folders to ignore (e.g. `node_modules`, `.git`, `build`)
- **Configurable thread count** — control parallelism via a CLI flag
- **Run from anywhere** — uses `getcwd()` to resolve the working directory, so it works from any location once installed globally
- **Minimal dependencies** — pure C with only POSIX APIs (`pthreads`, `dirent`, `stat`)

## Getting Started

### Prerequisites

- A C compiler (`gcc` recommended)
- POSIX-compatible system (Linux, macOS, WSL)
- `make`

### Build

```bash
make
```

### Run

```bash
make run
```

Or run the binary directly with a custom thread count:

```bash
./happy-lines --threads=4
```

When launched, the program will prompt you to enter folder names to exclude from the count. Type folder names one at a time, then type `exit` when done:

```
Enter the folders to ignore (exit to stop):
node_modules
.git
exit
```

It will then recursively count all lines and print the result along with the time taken:

```
Total happy lines count: 48523
Time taken: 0.012000 seconds
```

### Install Globally (optional)

To use `happy-lines` from any directory, copy the compiled binary to a location on your `PATH`.

**macOS / Linux:**

```bash
sudo cp happy-lines /usr/local/bin/
```

Then simply `cd` into any project and run:

```bash
happy-lines --threads=4
```

### Clean

```bash
make clean
```

## How It Works

1. Parses CLI arguments for the thread count (`--threads=N`, defaults to 1 if omitted).
2. Prompts the user for folders to exclude.
3. Enumerates all top-level directories in the current working directory.
4. Splits directories evenly across the requested number of threads.
5. Each thread recursively walks its assigned directories, counting newlines in every regular file.
6. Files directly in the current directory are counted separately on the main path.
7. Results from all threads are summed and printed alongside elapsed time.

## Project Structure

```
happy-lines/
├── happy-lines.c   # Main program — threading, directory walking, line counting
├── argparser.c     # CLI argument parser (--flag=value style)
├── Makefile        # Build configuration
├── LICENSE         # MIT License
└── README.md
```

## Planned Features

- More advanced TUI
- Per-language LOC breakdown
- Richer command-line argument support

## License

This project is licensed under the [MIT License](LICENSE).

## Contributing

Contributions are always welcome! Feel free to open issues or submit pull requests.
