codefile – Python Project Packager

What Is codefile?

codefile is a Python package that scans your project directory
and packs everything into a single output file named:

    CodeFile

It includes:
- Project folder structure (tree view)
- File contents
- Total size (in MB)
- UTC creation timestamp

This is useful for:
- Creating project snapshots
- Sharing full source code as one file
- Simple archiving
- Backup purposes
- AI context


# Installation

If installed from source:

    pip install .

installing as a package:

    pip install codefile


# How To Use

Step 1:
Open terminal inside your project folder.

Step 2:
Run:

    codefile

Step 3:
After execution, a new file will appear:

    CodeFile

That file contains your full project snapshot.


What Gets Packed?
=====================================

✔ All regular files in the current directory  
✔ Subdirectories  
✔ File contents  
✔ Directory structure  

Automatically ignored:
- .git folder
- The generated CodeFile
- The package script itself
- Patterns listed in .gitignore (if present)


=====================================
Output File Structure
=====================================

The generated file contains:

ROOT_NAME <project_name>
CREATED_UTC <timestamp>
TOTAL_SIZE_MB <size>

START_STRUCTURE
<folder tree>
END_STRUCTURE

FILE_START <relative_path> <file_id>
<file content>
FILE_END <relative_path> <file_id>


=====================================
Technical Details
=====================================

- Uses SHA-256 hashing (first 12 characters as ID)
- Reads files in chunks (memory safe)
- Skips unreadable or broken files safely
- Writes output using UTF-8 encoding
- Handles errors without crashing

=====================================
Requirements
=====================================

- Python 3.8+
- No external dependencies (uses standard library only)
