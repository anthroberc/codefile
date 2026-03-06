import os
import hashlib
import datetime
import fnmatch
from pathlib import Path

def main():
    IO_CHUNK = 65536
    OUT_NAME = "CodeFile"
    SCRIPT_NAME = Path(__file__).name if '__file__' in globals() else "packager.py"

    def get_ignore_patterns(root):
        patterns = {OUT_NAME, SCRIPT_NAME, ".git", ".git/"}
        gitignore = root / ".gitignore"
        try:
            if gitignore.is_file():
                with gitignore.open("r", encoding="utf-8", errors="ignore") as f:
                    for line in f:
                        line = line.strip()
                        if line and not line.startswith("#"):
                            patterns.add(line)
        except Exception as e:
            print(f"Warning: Failed reading .gitignore: {e}")
        return list(patterns)

    def is_ignored(path, root, patterns):
        try:
            rel_path = path.relative_to(root).as_posix()
            parts = rel_path.split('/')
            for pattern in patterns:
                clean_pattern = pattern.rstrip('/')
                for part in parts:
                    if fnmatch.fnmatch(part, clean_pattern):
                        return True
                if fnmatch.fnmatch(rel_path, pattern):
                    return True
        except ValueError:
            return True  # If it fails relative resolution (e.g., weird symlink), ignore it
        return False

    def build_visual_tree(root, all_relative_paths):
        tree = {}
        for path_str in sorted(all_relative_paths):
            current = tree
            for part in path_str.split('/'):
                current = current.setdefault(part, {})

        lines = [f"{root.name}/"]
        def walk(node, prefix=""):
            items = sorted(node.items(), key=lambda x: (not x[1], x[0]))
            for i, (name, children) in enumerate(items):
                is_last = (i == len(items) - 1)
                connector = "\\-- " if is_last else "|-- "
                lines.append(f"{prefix}{connector}{name}{'/' if children else ''}")
                if children:
                    walk(children, prefix + ("    " if is_last else "|   "))
        walk(tree)
        return "\n".join(lines)

    # 1. Resolve Environment Safely
    try:
        root = Path.cwd().resolve()
    except Exception as e:
        return print(f"Fatal Error: Cannot resolve current directory: {e}")
    
    if not root.is_dir(): 
        return print(f"Fatal Error: {root} is not a valid directory.")

    patterns = get_ignore_patterns(root)
    all_rel_paths = []
    files_to_write = []
    total_bytes = 0
    
    print(f"Scanning directory: {root.name} ...")
    
    # 2. Safely Walk Directory
    for dirpath, dirnames, filenames in os.walk(root):
        try:
            dp = Path(dirpath)
            
            # Instantly skip git folder traversal
            if ".git" in dp.parts:
                continue
                
            for f in filenames:
                p = dp / f
                try:
                    if not p.is_file():
                        continue
                        
                    rel_path = p.relative_to(root).as_posix()
                    all_rel_paths.append(rel_path)
                    
                    if not is_ignored(p, root, patterns):
                        total_bytes += p.stat().st_size
                        files_to_write.append(p)
                except (OSError, ValueError):
                    continue  # Skip unreadable files or broken symlinks
        except Exception:
            continue

    print(f"Found {len(files_to_write)} files to pack. Building structure...")
    
    # 3. Safely Write Output
    try:
        tree_str = build_visual_tree(root, all_rel_paths)
        timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        out_file = root / OUT_NAME
        
        with out_file.open("w", encoding="utf-8", errors="replace", newline='\n') as out:
            out.write(f"ROOT_NAME {root.name}\n")
            out.write(f"CREATED_UTC {timestamp}\n")
            out.write(f"TOTAL_SIZE_MB {total_bytes / (1024*1024):.2f}\n\n")
            out.write("START_STRUCTURE\n" + tree_str + "\nEND_STRUCTURE\n\n")

            for f_path in files_to_write:
                try:
                    rel = f_path.relative_to(root).as_posix()
                    h = hashlib.sha256()
                    
                    # Read 1: Hash Calculation
                    with f_path.open("rb") as rb:
                        while chunk := rb.read(IO_CHUNK):
                            h.update(chunk)
                    f_id = h.hexdigest()[:12]

                    # Read 2: Content Writing
                    out.write(f"FILE_START {rel} {f_id}\n")
                    if f_path.stat().st_size == 0:
                        out.write("EMPTY")
                    else:
                        with f_path.open("rb") as rb:
                            while chunk := rb.read(IO_CHUNK):
                                out.write(chunk.decode("utf-8", errors="replace"))
                    out.write(f"\nFILE_END {rel} {f_id}\n\n")
                    
                except Exception as e:
                    print(f"Warning: Skipped reading content of {f_path.name} ({e})")
                    continue

        print(f"Success. Generated {OUT_NAME} ({total_bytes / (1024*1024):.2f} MB)")
        
    except Exception as e:
        print(f"Critical Failure writing output file: {e}")

if __name__ == "__main__":
    main()
