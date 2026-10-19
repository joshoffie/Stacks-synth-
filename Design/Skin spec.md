# Stacks skin: the image files

Stacks draws its interface from a folder of PNG files. Edit them in Photoshop,
export them with the same names and sizes, and the plug-in shows your version.
Any file you leave out keeps the built-in look, so you can work one piece at a
time.

## Working rules

- **PNG, 2x.** Every file is painted at twice its on-screen size (one screen
  pixel = two image pixels). The sizes below are image pixels.
- **Same name, same size, same frame count.** The file name is how Stacks finds
  the piece. Keep transparency where the current file has it.
- **White pieces get coloured by the app.** Blocks, the slab, the links and the
  generic tile are painted in white/grey; Stacks multiplies them by each
  sound's colour. Paint shading in greys, not in colour.
- **Nine-slice pieces stretch.** Cards, buttons, panels: the corners stay as
  painted, the edges and the middle stretch. Keep details within the margin
  listed; the middle should be flat or a smooth gradient.
- **Where to put them while testing:** `~/Music/Stacks Patches/skin/` on a Mac
  running Stacks. Reopen the plug-in window to reload. The shipped set lives
  in the app bundle.
- `skin.json` in the same folder sets the scale and the slice margins; the
  exported one is correct for these sizes. Set `"knobArc": false` if your knob
  filmstrip already includes the value ring and you don't want the coloured
  arc drawn over it.

## The files

Sizes are W x H in image pixels (2x). "slice" = nine-slice margins left, top,
right, bottom in screen pixels.

### Knobs (filmstrips)
| File | Size | Notes |
|---|---|---|
| knob.png | 96 x 9696 | 101 frames of 96 x 96, top = minimum, bottom = maximum. The body only; the coloured arc is drawn by the app. Frame includes a 14 px margin for the arc. |
| knob-master.png | 88 x 8888 | 101 frames of 88 x 88, the header volume knob. |

### Buttons
| File | Size | Notes |
|---|---|---|
| button.png, button-hover.png, button-down.png, button-on.png | 128 x 48 | slice 8 8 8 8. "on" = toggled/selected. |
| button-bright.png | 128 x 48 | slice 8 8 8 8. The accent-filled call-to-action (Evolve, Grow). |
| tab-on.png, tab-hover.png | 160 x 48 | slice 8 8 8 8. Drawn behind the tab text; today only an underline. |
| toggle-off.png, toggle-on.png | 34 x 34 | the checkbox (Design wavetables). |
| combo.png | 160 x 44 | slice 8 8 20 8. Includes its own chevron at the right. |

### Slider
| File | Size | Notes |
|---|---|---|
| slider-track.png | 160 x 22 | slice 6 0 6 0 |
| slider-fill.png | 160 x 22 | slice 6 0 6 0, the filled part |
| slider-thumb.png | 24 x 24 | centred on the value |

### Surfaces
| File | Size | Notes |
|---|---|---|
| card.png | 128 x 128 | slice 10 10 10 10. Every section card. |
| inset.png | 128 x 128 | slice 8 8 8 8. Dark wells behind displays. |
| texteditor.png | 160 x 48 | slice 8 8 8 8. The preset name field. |
| popup.png | 160 x 160 | slice 10 10 10 10. Menus. |
| scrollbar-thumb.png | 16 x 80 | slice 4 4 4 4 |
| background.png | 3000 x 1720 | the whole window, stretched. |
| header.png | 3000 x 88 | the top strip (the wordmark and controls are drawn over it). |
| lab.png | 784 x 800 | slice 12 12 12 12. The right-hand panel. |

### Home
| File | Size | Notes |
|---|---|---|
| tile-pad.png, tile-keys.png, tile-bass.png, tile-lead.png, tile-pluck.png, tile-bell.png, tile-texture.png | 668 x 444 | one per kind, plus a `-hover` version of each (e.g. tile-pad-hover.png). Name and blurb are drawn over it. |
| tile.png, tile-hover.png | 668 x 444 | optional: one white tile for all kinds, tinted by the app. Used only when the per-kind file is missing. |

### Stack pieces (paint in white/grey; the app colours them)
| File | Size | Notes |
|---|---|---|
| block.png | 240 x 120 | front face plus the top and right faces (depth 20 px at 2x). |
| block-selected.png | 240 x 120 | the one being heard. |
| slab.png | 440 x 68 | the seed the sounds sit on (depth 16 px). |
| link-on.png, link-off.png | 44 x 28 | the arrow and switch between two effect cards. |

### Keyboard
| File | Size | Notes |
|---|---|---|
| key-white.png, key-white-hover.png, key-white-down.png | 56 x 116 | stretched to each key. |
| key-black.png, key-black-hover.png, key-black-down.png | 36 x 72 | |

## Regenerating the starter set

The shipped files are rendered from the code-drawn look, so they always match
what the plug-in shows without a skin:

```bash
cmake --build build.nosync --target StacksUiKit
./build.nosync/StacksUiKit_artefacts/Release/StacksUiKit --export-skin Skin 2
```

Render the screens with a skin folder to check it:

```bash
STACKS_SKIN_DIR="$PWD/Skin" ./build.nosync/StacksUiKit_artefacts/Release/StacksUiKit Design/screens 2
```
