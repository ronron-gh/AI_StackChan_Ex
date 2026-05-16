# Avatar framerate delay constants

## Purpose

Make the avatar loop delay and breath animation period explicit constants so the
relationship between the loop delay and the breath cycle is easy to maintain.

## Scope

- `lib/m5stack-avatar/src/Avatar.cpp`

## Main changes

- Add macro constants for the avatar loop delay, breath period multiplier, and
  derived breath period.
- Use the shared loop delay constant in both `drawLoop()` and `facialLoop()`.
- Use the derived breath period constant for the `millis()` based breath
  calculation.

## Design notes

- Define the breath period as the loop delay multiplied by 100, keeping the
  intended relationship visible when the loop delay is tuned.
- Limit the change to naming existing values; no task scheduling or drawing
  behavior should otherwise change.

## Verification

- Confirm the code builds syntactically.
- If practical, run a related PlatformIO build after the edit.
