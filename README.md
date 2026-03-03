# CodeFile Generator

A single-file C++17 command-line tool that scans any folder on your computer and produces a single deterministic snapshot file called `CodeFile`. That snapshot contains a full directory tree, the content of every file, and a SHA-256-based fingerprint for each one — all in plain UTF-8 text.

---

## Table of Contents

1. [What Does It Do?](#what-does-it-do)
2. [Requirements](#requirements)
3. [How to Compile](#how-to-compile)
4. [How to Run](#how-to-run)
5. [Understanding the Output File](#understanding-the-output-file)
6. [Full Example — Step by Step](#full-example--step-by-step)
7. [Error Messages Explained](#error-messages-explained)
8. [Frequently Asked Questions](#frequently-asked-questions)
9. [Technical Reference](#technical-reference)

---

## What Does It Do?

Imagine you have a project folder like this:

```
myproject/
    src/
        main.cpp
        utils/
            math.cpp
    docs/
        README.md
```

You run `codefile`, type the path to `myproject`, and the tool creates a single file called `CodeFile` inside `myproject`. That file contains:

- A short header (folder name, timestamp, total size)
- An ASCII drawing of the full directory tree
- The raw content of every file, wrapped in labeled start/end markers
- A 12-character SHA-256 fingerprint for each file

This is useful for archiving, code review, sharing a project snapshot in a single text file, or quickly auditing what a directory contains.

---

## Requirements

| Requirement | Detail |
|---|---|
| C++ compiler | GCC 8+, Clang 7+, or MSVC 2019+ |
| C++ standard | C++17 or later |
| Operating system | Linux, macOS, or Windows |
| External libraries | **None** — zero dependencies |

---

## How to Compile

Open a terminal, navigate to the folder that contains `CodeFile.cpp`, and run:

```bash
g++ -std=c++17 CodeFile.cpp -o codefile
```

That's it. You now have an executable called `codefile` (or `codefile.exe` on Windows).

**Optional: compile with optimizations**

```bash
g++ -std=c++17 -O2 CodeFile.cpp -o codefile
```

**On macOS with Apple Clang**

```bash
clang++ -std=c++17 CodeFile.cpp -o codefile
```

**On Windows with MSVC** (Developer Command Prompt)

```bash
cl /std:c++17 /EHsc CodeFile.cpp /Fe:codefile.exe
```

---

## How to Run

```bash
./codefile
```

The program will prompt you:

```
Enter absolute path to project folder:
```

Type (or paste) the **full absolute path** to the folder you want to scan, then press Enter.

**Example on Linux / macOS:**

```
Enter absolute path to project folder: /home/alice/projects/myapp
```

**Example on Windows:**

```
Enter absolute path to project folder: C:\Users\Alice\projects\myapp
```

After a moment, the tool exits silently with no output on success. The file `CodeFile` will have been created inside the folder you specified.

---

## Understanding the Output File

The generated `CodeFile` is a plain text file split into three sections.

### Section 1 — Header

```
ROOT_NAME: myapp
CREATED_UTC: 2026-03-03T10:45:00Z
TOTAL_SIZE_MB: 0.03
```

| Field | Meaning |
|---|---|
| `ROOT_NAME` | The name of the folder you scanned (last component of the path) |
| `CREATED_UTC` | When the file was generated, in UTC time (not your local timezone) |
| `TOTAL_SIZE_MB` | Combined size of all scanned files in megabytes, rounded to 2 decimal places |

### Section 2 — Directory Tree

```
START_STRUCTURE
myapp/
|-- docs/
|   \-- README.md
\-- src/
    |-- utils/
    |   \-- math.cpp
    \-- main.cpp
END_STRUCTURE
```

This is a human-readable ASCII map of the folder. Rules:

- Directories appear **before** files at each level
- Everything is sorted **alphabetically**
- `|--` means "there are more items after this one"
- `\--` means "this is the last item in this group"
- `|   ` is a vertical continuation line
- `    ` (4 spaces) means no continuation — you've reached the last branch

### Section 3 — File Blocks

Each file gets its own block:

```
FILE_START src/main.cpp 549fc4fff985
#include <iostream>
int main() {
    std::cout << "Hello, world!\n";
}
FILE_END src/main.cpp 549fc4fff985
```

| Part | Meaning |
|---|---|
| `FILE_START` | Marks the beginning of a file's content |
| `src/main.cpp` | Relative path from the root folder, using forward slashes |
| `549fc4fff985` | First 12 hex characters of the file's SHA-256 hash (the "fingerprint") |
| *(content)* | The exact raw bytes of the file — nothing added, nothing removed |
| `FILE_END` | Marks the end of the file's content |

The same relative path and fingerprint appear on both `FILE_START` and `FILE_END` so you can verify nothing was corrupted.

---

## Full Example — Step by Step

**1. Create a test project**

```bash
mkdir -p /tmp/demo/src/utils
echo '#include <iostream>
int main(){ std::cout << "Hi!\n"; }' > /tmp/demo/src/main.cpp

echo 'int add(int a, int b){ return a + b; }' > /tmp/demo/src/utils/math.cpp

echo '# My Project' > /tmp/demo/README.md
```

Your folder now looks like:

```
demo/
    README.md
    src/
        main.cpp
        utils/
            math.cpp
```

**2. Compile the tool**

```bash
g++ -std=c++17 CodeFile.cpp -o codefile
```

**3. Run it**

```bash
./codefile
Enter absolute path to project folder: /tmp/demo
```

**4. Inspect the result**

```bash
cat /tmp/demo/CodeFile
```

You will see something like:

```
ROOT_NAME: demo
CREATED_UTC: 2026-03-03T10:45:00Z
TOTAL_SIZE_MB: 0.00

START_STRUCTURE
demo/
|-- src/
|   |-- utils/
|   |   \-- math.cpp
|   \-- main.cpp
\-- README.md
END_STRUCTURE

FILE_START README.md a1b2c3d4e5f6
# My Project

FILE_END README.md a1b2c3d4e5f6

FILE_START src/main.cpp 549fc4fff985
#include <iostream>
int main(){ std::cout << "Hi!\n"; }

FILE_END src/main.cpp 549fc4fff985

FILE_START src/utils/math.cpp 6df5cb5a7846
int add(int a, int b){ return a + b; }

FILE_END src/utils/math.cpp 6df5cb5a7846

```

**5. Run it again**

Run it a second time on the same folder. Everything except `CREATED_UTC` will be **byte-for-byte identical**. The fingerprints, tree, paths, and file content blocks will never change as long as the files do not change.

---

## Error Messages Explained

If something goes wrong, the tool prints a message to `stderr` and exits with code `1`. The output file is never partially written — if any step fails, the temporary file is deleted first.

| Error message | What it means | How to fix it |
|---|---|---|
| `Error: empty path provided` | You pressed Enter without typing anything | Re-run and type the full path |
| `Error: cannot resolve path '...': ...` | The path contains `..` components that don't resolve, or the path doesn't exist | Double-check spelling; use an absolute path |
| `Error: path does not exist: ...` | The folder at that path was not found | Make sure the folder exists |
| `Error: path is not a directory: ...` | You pointed to a file, not a folder | Point to the folder that *contains* the files |
| `Error: directory not accessible: ...` | You don't have permission to read the folder | Check folder permissions (`ls -la`) |
| `Error: cannot create temporary file in: ...` | No write permission in the target folder | Check write permissions or run as appropriate user |
| `Error: cannot read file for hashing: ...` | A file exists but cannot be opened for reading | Check the file's permissions |
| `Error: cannot read file content: ...` | A file could be hashed but failed during content streaming | Disk error or file was removed mid-run |
| `Error: write failure ...` | Disk ran out of space or I/O error while writing | Free up disk space |
| `Error: cannot finalize CodeFile: ...` | The rename from `.tmp` to `CodeFile` failed | Check permissions; `CodeFile` may be locked |

---

## Frequently Asked Questions

**Q: Will it scan hidden files (dotfiles like `.gitignore`)?**

Yes. The tool does not skip hidden files. Any regular file in the directory tree will be included.

---

**Q: What files are excluded?**

- The `CodeFile` output file itself (so it doesn't include itself)
- The `CodeFile.tmp` working file
- Symbolic links (skipped silently)
- Directories themselves (only their contents are recorded)

---

**Q: What happens with empty files?**

Empty files are included. Their block will look like:

```
FILE_START path/to/empty.txt d41d8cd98f00
                              ← one blank line (the FILE_START line's newline + content separator)
FILE_END path/to/empty.txt d41d8cd98f00
```

The SHA-256 of an empty file is always `d41d8cd98f00b204e9800998ecf8427e`, so its 12-char ID is always `d41d8cd98f00`.

---

**Q: Does it handle binary files?**

Yes. The file content is streamed raw (binary mode). However, since `CodeFile` itself is a text file, embedding binary content inside it may produce unreadable characters when viewed in a text editor. The bytes are preserved exactly.

---

**Q: What if a file has no trailing newline?**

`FILE_END` is placed on the line immediately following the last byte of content — no extra newline is injected. The file content is preserved exactly as-is.

---

**Q: Is the output identical across different machines?**

Yes, **except for `CREATED_UTC`**. Given the same files and folder structure, the rest of the output is byte-for-byte identical across platforms, because:
- Files are always sorted alphabetically
- Path separators are always normalized to `/`
- SHA-256 is deterministic
- Floating-point MB is formatted with `std::fixed` (no scientific notation)

---

**Q: Why does it open each file twice (once for hashing, once for content)?**

To guarantee the ID matches the content actually written. The hash is computed first; if that fails, generation aborts before any content is written. This prevents a block where the ID doesn't match the content.

---

## Technical Reference

### Output Format (Formal)

```
ROOT_NAME: <folder_name>
CREATED_UTC: <YYYY-MM-DDTHH:MM:SSZ>
TOTAL_SIZE_MB: <N.NN>

START_STRUCTURE
<root_name>/
[|-- or \-- ]<name>[/]
...
END_STRUCTURE

FILE_START <rel/path> <12hex>
<raw file bytes>
FILE_END <rel/path> <12hex>

[repeated for each file]
```

### ID Algorithm

1. Open file in binary mode
2. Read in 64 KB chunks
3. Feed all bytes into SHA-256
4. Take the first 6 bytes of the 32-byte digest
5. Format as 12 lowercase hex characters

### Tree Symbols

| Symbol | Meaning |
|---|---|
| `\|--` | Non-last child |
| `\--` | Last child |
| `\|   ` | Vertical continuation (parent has more siblings) |
| `    ` | No continuation (parent was the last sibling) |

### Exit Codes

| Code | Meaning |
|---|---|
| `0` | Success — `CodeFile` was written |
| `1` | Failure — error printed to `stderr`, no output written |

### Compilation

```bash
g++ -std=c++17 CodeFile.cpp -o codefile
```

No flags, no libraries, no build system required.
