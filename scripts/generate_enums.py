#!/usr/bin/env python3
"""
Generate TypeScript enums from C++ header files.
Thi        # Remove common C++ enum prefixes (pattern: 2 or 3 uppercase letters followed by underscore)
        # Examples: HAE_, HSD_, CCM_, HR_, but NOT: WIFI_, USB_DEVICE_, etc.
        prefix_pattern = r'^[A-Z]{2,3}_'
        if re.match(prefix_pattern, ts_name):
            # Find the prefix and remove it
            prefix_match = re.match(prefix_pattern, ts_name)
            if prefix_match:
                ts_name = ts_name[prefix_match.end():] parses C++ enum definitions and creates corresponding TypeScript enums.
Can be used both as standalone script and as PlatformIO pre-action.
"""

import re
import os
import sys
from pathlib import Path
from typing import Dict, List, Tuple

def parse_cpp_enum(content: str, enum_name: str) -> Tuple[Dict[str, str], Dict[str, str]]:
    """Parse a C++ enum and return values and comments."""
    
    # Pattern to match enum definition
    pattern = rf'typedef\s+enum\s+{enum_name}\s*\{{([^}}]+)\}}'
    match = re.search(pattern, content, re.DOTALL)
    
    if not match:
        return {}, {}
    
    enum_body = match.group(1)
    
    # Parse enum values
    values = {}
    comments = {}
    
    for line in enum_body.split('\n'):
        line = line.strip()
        if not line or line.startswith('//'):
            continue
            
        # Extract comment if present
        comment = ""
        if '//' in line:
            parts = line.split('//', 1)
            line = parts[0].strip()
            comment = parts[1].strip()
        
        # Remove trailing comma
        line = line.rstrip(',')
        
        if '=' in line:
            name, value = line.split('=', 1)
            name = name.strip()
            value = value.strip()
            
            # Skip MIN/MAX boundary values
            if name.endswith('_MIN') or name.endswith('_MAX'):
                continue
                
            values[name] = value
            if comment:
                comments[name] = comment
        elif line and not line.startswith('}'):
            # Enum without explicit value
            name = line.strip()
            if not name.endswith('_MIN') and not name.endswith('_MAX'):
                values[name] = None
                if comment:
                    comments[name] = comment
    
    return values, comments

def generate_typescript_enum(enum_name: str, values: Dict[str, str], comments: Dict[str, str]) -> Tuple[str, str]:
    """Generate TypeScript enum from parsed values. Returns (ts_enum_content, ts_enum_name)."""
    
    # Convert C++ enum name to TypeScript enum name
    # Only remove the trailing "_t" suffix, keep semantic prefixes like "genius_"
    clean_name = enum_name.replace('_t', '') if enum_name.endswith('_t') else enum_name
    
    # Convert to PascalCase while preserving semantic meaning
    ts_enum_name = ''.join(word.capitalize() for word in clean_name.split('_'))
    
    lines = [f"export enum {ts_enum_name} {{"]
    
    for name, value in values.items():
        # Convert C++ constant name to TypeScript
        ts_name = name
        
        # Remove common C++ enum prefixes (pattern: 2 or 3 uppercase letters followed by underscore)
        # Examples: HR_, HAE_, HSD_, CCM_, but NOT: WIFI_, USB_DEVICE_, etc.
        prefix_pattern = r'^[A-Z]{2,3}_'
        if re.match(prefix_pattern, ts_name):
            # Find the prefix and remove it
            prefix_match = re.match(prefix_pattern, ts_name)
            if prefix_match:
                ts_name = ts_name[prefix_match.end():]
        
        # Convert to PascalCase
        ts_name = ''.join(word.capitalize() for word in ts_name.split('_'))
        
        # Add comment if available
        if name in comments:
            lines.append(f"  /** {comments[name]} */")
        
        if value is not None:
            lines.append(f"  {ts_name} = {value},")
        else:
            lines.append(f"  {ts_name},")
    
    lines.append("}")
    lines.append("")
    
    return '\n'.join(lines), ts_enum_name

def generate_enums_main(project_dir_override=None):
    """Main function to generate TypeScript enums."""
    
    if project_dir_override:
        project_dir = Path(project_dir_override)
    else:
        project_dir = Path(__file__).parent.parent
    
    src_dir = project_dir / "src"
    output_file = project_dir / "interface" / "src" / "lib" / "types" / "enums.ts"
    
    # Find all header files in src directory (including subdirectories)
    header_files = []
    for header_file in src_dir.rglob("*.h"):
        # Skip library headers (lib directory)
        if "lib" not in header_file.parts:
            header_files.append(header_file)
    
    if not header_files:
        print("No header files found in src directory")
        return False
    
    all_enums = []
    all_enums.append("// Auto-generated TypeScript enums from C++ headers")
    all_enums.append("// DO NOT EDIT MANUALLY - This file is generated by scripts/generate_enums.py")
    all_enums.append("// Run 'python scripts/generate_enums.py' to regenerate")
    all_enums.append("")
    
    enum_count = 0
    processed_enums = []  # Track all processed enums for summary
    
    for header_file in sorted(header_files):
        relative_path = header_file.relative_to(src_dir)
        print(f"Processing {relative_path}...")
        
        try:
            with open(header_file, 'r', encoding='utf-8') as f:
                content = f.read()
        except Exception as e:
            print(f"  Warning: Could not read {relative_path}: {e}")
            continue
        
        # Find all enum definitions
        enum_pattern = r'typedef\s+enum\s+(\w+)\s*\{'
        enum_matches = re.findall(enum_pattern, content)
        
        file_enum_count = 0
        for enum_name in enum_matches:
            values, comments = parse_cpp_enum(content, enum_name)
            if values:
                ts_enum_content, ts_enum_name = generate_typescript_enum(enum_name, values, comments)
                all_enums.append(f"// From {relative_path}")
                all_enums.append(ts_enum_content)
                file_enum_count += 1
                enum_count += 1
                
                # Track the enum mapping for output
                processed_enums.append((relative_path, enum_name, ts_enum_name))
                print(f"  ✓ {enum_name} → {ts_enum_name}")
        
        if file_enum_count == 0:
            print(f"  No enums found")
    
    # Create output directory if it doesn't exist
    output_file.parent.mkdir(parents=True, exist_ok=True)
    
    # Write the combined TypeScript file
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(all_enums))
    
    print(f"\n{'-' * 60}")
    print(f"Generated {enum_count} TypeScript enum(s) in {output_file}")
    
    if processed_enums:
        print(f"\nEnum Mapping Summary:")
        print(f"{'C++ Enum':<25} → {'TypeScript Enum':<20} {'Source File'}")
        print(f"{'-' * 25}   {'-' * 20} {'-' * 30}")
        for file_path, cpp_enum, ts_enum in processed_enums:
            print(f"{cpp_enum:<25} → {ts_enum:<20} {file_path}")
    
    return True


def generate_enums_platformio_action(source, target, env):
    """PlatformIO pre-action to generate TypeScript enums."""
    project_dir = env["PROJECT_DIR"]
    
    print("=" * 50)
    print("Generating TypeScript enums from C++ headers...")
    print("=" * 50)
    
    try:
        success = generate_enums_main(project_dir)
        if success:
            print("✓ TypeScript enums generated successfully")
        else:
            print("✗ Failed to generate enums")
    except Exception as e:
        print(f"✗ Error running enum generation: {e}")
        # Don't fail the build, just warn


# Standalone execution
def main():
    """Main function for standalone execution."""
    generate_enums_main()


# PlatformIO integration
try:
    # This will only work when script is imported by PlatformIO
    Import("env")
    # Register the pre-action when imported by PlatformIO
    env.AddPreAction("buildprog", generate_enums_platformio_action)
    print("PlatformIO enum generation pre-action registered")
except:
    # Running as standalone script or PlatformIO not available
    pass


if __name__ == "__main__":
    main()
