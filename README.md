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

The first XT artifact is the refactor plan:

- `DFAF_XT_REFACTOR_PLAN.md`

Next implementation step:

- port the DFAF core into XT as a separate codebase
- preserve build architecture and DSP baseline
- then expand the sequencer from `8x2` to `16x5`
