# SnipLite v0.1.0 Release Notes

Date: 2026-03-16

## Highlights

- Improved sidebar interaction model for edge reveal, docking, and hold behavior.
- Added pin button support near the top controls and persisted pin state across restarts.
- Added close confirmation dialog to reduce accidental app exits.

## Capture Experience

- Region capture now uses a frozen-frame style interaction for a steadier selection flow.
- Fixed high-DPI selection scaling issues.
- Reduced selection jitter during region capture updates.

## Settings and Tray Behavior

- Fixed the "hide sidebar on region capture" option behavior.
- Kept tray left-click flow aligned with expected sidebar summon behavior.

## Build and Project Stability

- Recovered and stabilized CMake project configuration.
- Verified both Debug and Release builds in the current Windows/Qt environment.

## Commit Reference

- Main release commit: 80232dd (`feat: refine sidebar interactions and persist pin state`)