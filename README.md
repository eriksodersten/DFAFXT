# DFAF XT

`DFAF XT` is a standalone follow-up project built on the technical foundation of `DFAF`, with a larger sequencer-first panel, expanded modulation, and a broader percussion palette.

## Project Direction

- separate project and repository from `DFAF`
- based on the `DFAF` core architecture, not a brand-new synth engine
- 16-step sequencer with `Pitch`, `Velocity`, `Mod A`, `Mod B`, `Mod C`
- transport and clock integrated directly with the sequencer area
- richer modulation and sound design while keeping a fixed, musical, one-knob-per-function workflow

## Reference Project

Technical reference lives in:

- `/Users/eriksode/Projects/DFAF/`

New XT work belongs in:

- `/Users/eriksode/Projects/DFAFXT/`

## Current Status

Current XT artifacts:

- `DFAF_XT_REFACTOR_PLAN.md`
- initial JUCE/CMake port of the DFAF core into the XT repo
- 16-step sequencer with `Pitch`, `Velocity`, `Mod A`, `Mod B`, `Mod C`
- `Mod A/B/C` exposed as held sequencer modulation sources in the XT patch system

Next implementation step:

- add dedicated XT modulation destinations and amounts for `Mod A/B/C`
- reshape the top panel toward the XT sketch sections
- decide whether XT should expose the patchbay directly or move toward a hybrid modulation UI
