# wkTrackTime

`wkTrackTime` is a WormKit module for Worms Armageddon 3.8.x that tracks how long each worm spends on its turn and shows the collected times in a compact in-game overlay above the global timer.

## What It Does

- Reads the active turn timer from `CTaskTurnGame` using offsets `0x188` and `0x18C`.
- Detects the currently active worm and stores the elapsed time when the timer stops.
- Treats turn end consistently across normal end-of-turn cases, including drowning, losing control after impact, and manual turn skip.
- Groups recorded times by player and shows them in turn-start order.
- Renders everything with native W:A textboxes so the overlay visually matches the game's HUD.

## Overlay Behavior

- The overlay is anchored in the lower-left HUD area, directly above the native global timer.
- Players are stacked vertically.
- Each player name is shown in the team color.
- Recorded times are shown underneath the player name, split into rows of two values separated by `/`.
- Name box borders and time box borders follow the same team color styling used by the game HUD.
- Only the active player's name is allowed to blink; the recorded time boxes stay stable.

## Chat Commands

- `/enable tracktime`
- `/disable tracktime`
- `/enabletracktime`
- `/disabletracktime`
- `/resettracktime`

## Included Build

The repository includes the latest compiled module at the project root:

- `wkTrackTime.dll`
- `wkTrackTime.ini`

Copy those files into your Worms Armageddon directory, for example:

`C:\Program Files\Worms Armageddon\Team17\Worms Armageddon`

## Building

This project uses CMake and builds a Win32 DLL.

Example build flow:

```powershell
cmake -S . -B build-win32 -A Win32
cmake --build build-win32 --config Release
```

After a successful build, copy `build-win32\Release\wkTrackTime.dll` over the root `wkTrackTime.dll` if you want to refresh the bundled release artifact before publishing.

## Notes

- The module assumes the scheme uses a real turn timer.
- Recorded times are reset when the round state is rebuilt or when `/resettracktime` is used.
- Overlay placement can be fine-tuned in `wkTrackTime.ini` with `OverlayLeftPixels` and `OverlayBottomPixels`.
