# UI, Orientation & Input

> Deep reference for [CLAUDE.md](../../CLAUDE.md). All rendering goes through the
> `GUI`/UITheme macro; all input goes through logical buttons. Hardcoded screen
> dimensions or raw hardware button indices are bugs.

## Orientation-Aware Logic
* No Hardcoding: Never assume 800 or 480. Use renderer.getScreenWidth() and renderer.getScreenHeight().
* Viewable Area: Use renderer.getOrientedViewableTRBL() to stay within physical bezel margins.

## Logical Button Mapping

**Source**: [src/MappedInputManager.cpp:20-55](../../src/MappedInputManager.cpp)

Constraint: Physical button positions are fixed on hardware, but their logical functions change based on user settings and screen orientation.

**Button Categories**:
1. **Physical Fixed** (Up/Down side buttons):
   - `Button::Up` → Always `HalGPIO::BTN_UP`
   - `Button::Down` → Always `HalGPIO::BTN_DOWN`

2. **User Remappable** (Front buttons):
   - `Button::Back` → Maps to `SETTINGS.frontButtonBack` (hardware index)
   - `Button::Confirm` → Maps to `SETTINGS.frontButtonConfirm`
   - `Button::Left` → Maps to `SETTINGS.frontButtonLeft`
   - `Button::Right` → Maps to `SETTINGS.frontButtonRight`

3. **Reader-Specific** (Page navigation with optional swap):
   - `Button::PageBack` → Uses side button (swappable via `SETTINGS.sideButtonLayout`)
   - `Button::PageForward` → Uses side button (swappable)

**Implementation**:
- Activities use **logical buttons** (e.g., `Button::Confirm`)
- `MappedInputManager` translates to **physical hardware buttons**
- User can remap front buttons in settings
- Orientation changes handled separately by renderer coordinate transforms

**Rule**: Always use `MappedInputManager::Button::*` enums, never raw `HalGPIO::BTN_*` indices (except in ButtonRemapActivity).

## UITheme (The GUI Macro)
* Rule: All UI rendering must go through the GUI macro (UITheme).
* Do not hardcode fonts, colors, or positioning. This ensures orientation-aware layout consistency.

> User-facing text must use the `tr()` macro — see
> [hardware-constraints.md](hardware-constraints.md) → Resource Protocol rule 5,
> and the i18n workflow in [generated-files.md](generated-files.md).
