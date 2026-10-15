# Sound-page cleanup build and software evidence

Clean compiled source: `728a0868836b57651ba4f76b39a5f013c47ee363`.
Firmware SHA256: `b3f31704d063f63d79b8019b5e166e2677bf39df9e9a9b4bddb86a7a887ff99a`.
Firmware package: 611255 bytes. Application: 580436 / 581564 bytes (1128 spare).
RAM: 91448 / 98304; pool: 329932 / 344064. Required reserves remain intact.

The firmware and corresponding source bundle were verified locally with the
recovery-enabled development target. No website publisher was invoked and no
physical instrument was accessed. The normal installer remains Trial 04; the
separate drum preview remains Trial 05. The source stays on
`next/sound-page-cleanup`. Later evidence/documentation commits do not change
the compiled application.

## Checks completed

- 410 actual native panel/LCD checks, also passing GCC ASan/UBSan. These cover
  all twelve engines, conditional Analog/Digital/TRIO/Noise/PHYS modes,
  preserved hidden amounts, exact TIME/PITCH IDs and MIDI/lock ownership,
  visible hints, first-key playing, recorder/manual-save priority, page repair
  and Trial 05 drum copying/grid/edit navigation.
- 23 original paired production renders match the prior report exactly.
- 56 additional paired production renders validate Digital algorithms 0/7 and
  all sixteen TRIO wave sets for ENV/LFO PWM and static pulse width. All 112
  renders are non-silent; all expected conditions pass.
- Actual production WASM/AudioWorklet tests verify FM6's two visible ENV
  columns, inert empty columns, working LCD edits and audible key playing,
  preserved musical events, repeated ENV, Home and Analog ADSR restoration.
  The broader browser instrument suite and six drum browser scenarios pass.
- Existing Home (115), drum navigation (92), kit UI (330), individual drum
  sound/DSP (2609), actual drum-edit/save/boot (9), recovery runtime (306) and
  manual-save modal checks pass.
- The upstream non-JamBox UI suite, including 20000 random frames, passes.
- Identity rejection tests (12) pass. The canonical saved identity is unchanged;
  only an exact reviewed UI catalog alias was added. A fresh cross-version run
  at the clean compiled revision passes 34 cold restores and produces identical
  old/current 1 MiB synthetic NOR images for the same seeded session.
- Clean target build, RAM/pool reserves, HAL-only register accesses and local
  corresponding-source/container/ELF/memory packaging gates pass.

`validation.json` records actual log tails rather than inferred outcomes.
`cross-version.json` lists the exact unchanged source/asset hashes and separate
cold processes. `build-receipt.json` binds the compiler, SDK, generated inputs,
commands and output hashes. `memory.json` records the executable regions.
Screenshots are actual 240×240 native/browser framebuffers, with host icons
on (the hardware profile has icons off).

## Source and save compatibility review

The parameter enums, actual TP/GP definitions and all engine/voice/DSP source
are unchanged. The UI page table is transient; its indexes are not serialized.
The new FM ENV controls point directly to existing P_E4/P_ED_PIT with their
original ranges/defaults. Inapplicable modulation columns retain stored amounts.
Generated audio assets, engine/kit/preset order, recipes, SYN layout, project,
sidecar codec/runtime and canonical identity remain unchanged. The reviewed
catalog digest is `40fa0f6dad7a6bb464fad11f2343b668e1a68c56725a8132ba85146399590673`.
Unknown future catalog or asset changes still fail validation.

Audio hash matches support unchanged rendering under the documented probes;
they are not exhaustive listening or hardware validation. Physical FM-1
behavior and owner acceptance of the new controls remain unverified. No new
installation or deliberate power-interruption test is requested here.
