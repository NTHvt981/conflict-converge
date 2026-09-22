#!/usr/bin/env python3
"""
Bootstrap script for Conflict Converge project.
Pulls all Git dependencies into the deps/ folder.
"""

import json
import os
import shutil
import stat
import subprocess
import time
from pathlib import Path


def remove_tree(path: Path, attempts: int = 5) -> None:
    """Remove a directory tree on Windows, clearing read-only attributes.

    Git marks pack files under .git/objects read-only, which makes a plain
    shutil.rmtree fail with PermissionError. Retry a few times to ride out
    transient locks (antivirus, indexers, open handles).
    """
    def _on_error(func, failed_path, exc_info):
        os.chmod(failed_path, stat.S_IWRITE)
        func(failed_path)

    last_error = None
    for attempt in range(attempts):
        try:
            shutil.rmtree(path, onerror=_on_error)
            return
        except PermissionError as e:
            last_error = e
            if attempt < attempts - 1:
                time.sleep(0.5)
    raise last_error


def load_dependencies():
    """Load dependencies from libs_deps.json."""
    # Get absolute path based on current working directory
    script_dir = Path(__file__).parent.resolve()
    libs_deps_path = script_dir / "libs_deps.json"
    
    with open(libs_deps_path, "r") as f:
        deps_data = json.load(f)
    
    return deps_data


def pull_git_repo(dep_info: dict):
    """Pull a git repository into the specified destination."""
    name = dep_info["name"]
    git_source = dep_info.get("git_repo", dep_info.get("git_source"))
    dest = dep_info["dest"]
    git_tag = dep_info.get("git_tag")
    depth = dep_info.get("depth") # optional shallow clone (large repos: protobuf)

    # Use script directory as base for dest path
    script_dir = Path(__file__).parent.resolve()
    full_dest = script_dir / dest
    
    print(f"\n[>] Pulling {name} from {git_source}")

    # Remove existing folder if exists to get clean checkout
    if full_dest.exists():
        print(f"  [!] Removing existing folder: {full_dest}")
        try:
            remove_tree(full_dest)
        except OSError as e:
            print(f"  [-] Could not remove {full_dest}: {e}")
            print("      Close any program using it (editor, build, explorer) and re-run.")
            return False

    # Create parent directory
    full_dest.parent.mkdir(parents=True, exist_ok=True)

    try:
        # Clone the repository with specific tag/branch
        clone_cmd = ["git", "clone", git_source, str(full_dest)]
        if depth:
            clone_cmd.append("--depth")
            clone_cmd.append(str(depth))
        if git_tag:
            clone_cmd.append("--branch")
            clone_cmd.append(git_tag)
        
        print(f"  [>] Running: {' '.join(clone_cmd)}")
        subprocess.run(
            clone_cmd,
            check=True,
            capture_output=False
        )

        print(f"[+] {name} successfully cloned to {full_dest}")
        return True

    except subprocess.CalledProcessError as e:
        print(f"[-] Failed to clone {name}: {e}")
        
        # Retry logic for cloning
        max_retries = 3
        for attempt in range(max_retries):
            print(f"  [!] Retrying {name} (attempt {attempt + 1}/{max_retries})...")
            
            try:
                clone_cmd = ["git", "clone", git_source, str(full_dest)]
                if depth:
                    clone_cmd.append("--depth")
                    clone_cmd.append(str(depth))
                if git_tag:
                    clone_cmd.append("--branch")
                    clone_cmd.append(git_tag)
                
                print(f"  [>] Running: {' '.join(clone_cmd)}")
                subprocess.run(
                    clone_cmd,
                    check=True,
                    capture_output=False
                )
                
                print(f"[+] {name} successfully cloned to {full_dest}")
                return True
                
            except subprocess.CalledProcessError as retry_error:
                if attempt < max_retries - 1:
                    time.sleep(2)  # Wait 2 seconds before retry
                    continue
                else:
                    print(f"[-] Failed to clone {name} after {max_retries} attempts: {retry_error}")
                    return False

    return False


def main():
    """Main bootstrap function."""
    print("=" * 60)
    print("Conflict Converge - Bootstrap Script")
    print("=" * 60)
    
    # Load dependencies from libs_deps.json
    deps = load_dependencies()
    
    # Pull all git repositories
    success_count = 0
    for dep in deps:
        if pull_git_repo(dep):
            success_count += 1
    
    print(f"\n[+] Successfully pulled {success_count}/{len(deps)} dependencies")
    
    print("\n" + "=" * 60)
    print("Bootstrap complete!")
    print("=" * 60)
    print("\nNext steps:")
    print("1. Verify all dependencies are correctly installed")
    print("2. Run 'premake5 vs2022' to generate project files")
    print("3. Build raylib-static first: msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64")
    print("4. Then build the game: msbuild prj/ConflictConverge.sln /p:Configuration=Release /p:Platform=x64")


if __name__ == "__main__":
    main()
