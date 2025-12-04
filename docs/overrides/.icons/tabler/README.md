# Tabler Icons Integration Guide

This directory contains custom Tabler Icons in SVG format for use with MkDocs Material.

## How to Add/Fix Icons

### 1. Download Icon from Tabler
Visit [tabler.io/icons](https://tabler.io/icons) and find the icon you need. Copy the SVG code.

### 2. Clean the SVG

The original Tabler SVG looks like this:
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" 
     fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" 
     stroke-linejoin="round" class="icon icon-tabler icons-tabler-outline icon-tabler-example">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M12 5l0 14"/>
  <path d="M5 12l14 0"/>
</svg>
```

**Transform it to:**
```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><g fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 5l0 14"/><path d="M5 12l14 0"/></g></svg>
```

### 3. Required Changes

- ❌ **Remove** `width="24" height="24"` attributes
- ❌ **Remove** `fill="none" stroke="..." stroke-width="..."` from `<svg>` tag
- ❌ **Remove** `class="..."` attribute
- ❌ **Remove** the invisible rectangle path: `<path stroke="none" d="M0 0h24v24H0z" fill="none"/>`
- ❌ **Remove** `z` commands from circle arc paths (e.g., change `d="...4 0z"` to `d="...4 0"`)
- ✅ **Add** `<g fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">` wrapper
- ✅ **Wrap** all `<path>` elements inside the `<g>` element
- ✅ **Close** with `</g></svg>`
- ✅ **Save** using UTF-8 encoding **without BOM** (use `[System.Text.UTF8Encoding]::new($false)` in PowerShell)

### 4. Save the File

**Important:** Save as `icon-name.svg` in this directory (`docs/overrides/.icons/tabler/`) using UTF-8 encoding **without BOM**.

In PowerShell:
```powershell
[System.IO.File]::WriteAllText("docs\overrides\.icons\tabler\icon-name.svg", '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><g fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">...</g></svg>', [System.Text.UTF8Encoding]::new($false))
```

**DO NOT use** `Out-File -Encoding UTF8` as it adds a BOM that causes rendering issues.

### 5. Use in Documentation

Reference the icon in your markdown files:
```markdown
:tabler-icon-name:
```

## Why These Changes?

- The `<g>` wrapper ensures stroke attributes properly inherit to all child paths
- `fill="none"` prevents circles and shapes from appearing filled/solid
- Removing `z` commands from circle arcs prevents them from being filled (they create closed paths)
- `stroke="currentColor"` makes icons adapt to light/dark themes automatically
- Removing the invisible rectangle path prevents rendering conflicts
- UTF-8 without BOM prevents `&#xFEFF;` (Zero Width No-Break Space) from appearing before icons, which causes incorrect flexbox gap rendering

## Common Issues

**Problem:** Circles appear filled instead of outlined  
**Solution:** 
1. Make sure the `<g>` tag has `fill="none"` and wraps all paths
2. Remove `z` commands from circle arc paths (e.g., `d="M14 6a2 2 0 1 0 -4 0a2 2 0 0 0 4 0z"` → `d="M14 6a2 2 0 1 0 -4 0a2 2 0 0 0 4 0"`)

**Problem:** Icon doesn't show up  
**Solution:** Check that the filename matches what you use in `:tabler-filename:` (without .svg extension)

**Problem:** Icon color doesn't match text  
**Solution:** Ensure `stroke="currentColor"` is set on the `<g>` tag

**Problem:** Extra space/gap appears before the icon in navigation  
**Solution:** File has UTF-8 BOM. Re-save using `[System.Text.UTF8Encoding]::new($false)` in PowerShell
