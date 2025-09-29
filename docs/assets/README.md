# Documentation Assets

This directory contains all assets referenced in the Genius Gateway documentation.

## Directory Structure

```
assets/
├── images/                    # Screenshots, diagrams, photos
│   ├── hardware/              # Hardware photos, PCB images, component photos
│   ├── software/              # UI screenshots, web interface images
│   ├── diagrams/              # System diagrams, flowcharts, architecture
│   └── logos/                 # Project logos, icons, branding
├── downloads/                 # Downloadable files for users
│   ├── datasheets/            # Component datasheets (PDF)
│   ├── schematics/            # Schematic PDFs, design files
│   ├── gerbers/               # PCB manufacturing files (ZIP)
│   └── firmware/              # Firmware binaries, bootloaders
└── media/                     # Videos, animations, large media files
```

## Usage Guidelines

### File Naming Convention
- Use lowercase with hyphens: `gateway-assembled-top-view.jpg`
- Include version numbers: `schematic-v1.2.pdf`
- Be descriptive: `cc1101-module-pinout.png`

### File Formats
- **Images**: PNG for screenshots, JPG for photos, SVG for diagrams
- **Documents**: PDF for datasheets and schematics
- **Archives**: ZIP for gerber files and collections
- **Large files**: Consider compression and GitHub file size limits

### Referencing in Markdown
```markdown
# Images
![Alt text](assets/images/hardware/gateway-assembled.jpg)

# Downloads with links
[Schematic PDF](assets/downloads/schematics/genius-gateway-schematic-v1.2.pdf)

# Figures with captions
<figure markdown>
  ![PCB Layout](assets/images/hardware/pcb-layout.png){ width="600" }
  <figcaption>Complete PCB Layout</figcaption>
</figure>
```

## File Size Considerations

- **GitHub limit**: 100MB per file, 1GB per repository recommended
- **Optimize images**: Use appropriate compression for web viewing
- **Large files**: Consider using Git LFS or external hosting for very large assets
- **Gerbers/Manufacturing**: Keep original files in `cae/` directory, exports here

## License Information

Assets in this directory are subject to the project's multi-license structure:

- **Hardware images/files**: CC BY-SA 4.0
- **Software screenshots**: MIT (for interface) / GPL v3 (for backend)
- **Documentation images**: CC BY 4.0
- **Third-party datasheets**: Subject to manufacturer's license terms