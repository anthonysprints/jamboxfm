# JamBoxFM

Agreed product direction · October 13, 2026

Status: design brief. Firmware and browser prototype are not implemented yet.

## Purpose

A hands-on instrument for scrappy band jams, dusty beats, and big synth hooks on the M-VAVE FM-1.

Make it easy to start a satisfying groove with little music-theory knowledge, then keep playing, recording, and shaping sounds. A useful design target is finding a groove within a minute.

The owner currently uses Sloop. Its groove-making and sound exploration are appealing, but its sounds, visual feel, and learning curve do not fit well. JamBoxFM should make those experiences more approachable and give them a stronger musical identity.

## Musical direction

| Reference | Qualities to explore |
| --- | --- |
| The Strokes | Tight drums, driving bass, crunchy chord stabs, short repeating riffs |
| MF DOOM | Dusty drums, loose swing, warm bass, jazzy chords, unusual textures |
| The Killers | Punchy beats, pulsing bass, bright synth melodies, wide chorus and delay |

These references guide the sound and playing experience. For the guitar-oriented material, start with crunchy, percussive synth textures that fill a rhythm-guitar role. Convincing guitar modeling is outside the first prototype.

## Four musical parts

- **Drums:** playable kits, editable starting grooves, individual hits, fills, and mute.
- **Bass:** simple riff recording and immediate access to useful bass sounds.
- **Chords:** playable harmony with optional one-key chords and phrase recording.
- **Lead:** melodies, hooks, and textures to play over the other parts.

These are musical roles, not promises of separate hardware engines or unlimited polyphony. Voice allocation and the available DSP budget must support the complete jam.

## Starting palettes

| Palette | Starting character |
| --- | --- |
| Dusty | Dusty drums, mellow or jazzy chords, warm bass, wobbly lead textures |
| Garage | Straight drum grooves, driving bass, short crunchy chords, rough-edged riffs |
| Neon | Punchy drums, pulsing bass, bright leads, chorus and delay |

Each palette provides matching sounds and a suggested groove. Every part remains editable, and sounds can be mixed across palettes. The starting sounds should be enjoyable before any parameter editing.

## Main playing loop

1. Choose a palette and start a beat.
2. Play a few chords and capture a short loop.
3. Add a bassline.
4. Play a lead or texture over the loop.
5. Shape sounds, mute parts, add a fill, or make a variation without losing the original idea.

Undo, recording state, selected part, and mute state should be easy to find. Common playing actions should require little menu navigation. Exact assignments to the FM-1's existing buttons and encoders still need to be designed and tried.

## Optional playing assistance

Provide a clearly labeled **Assist** toggle in a quick menu, with individual controls inside it:

| Control | Settings | Behavior |
| --- | --- | --- |
| One-key chords | On / Off | A single key triggers a complete chord when enabled |
| Stay in key | On / Off | Maps newly played melody notes to the selected scale when enabled |
| Timing correction | Off / Gentle / Tight | Controls how strongly newly recorded events align to the timing grid |

Assistance must be easy to disable. Store its settings with each project and allow part-specific choices. The master toggle should preserve the individual choices so they can be restored when assistance is enabled again.

The proposed starting defaults are chord assistance enabled for the chord part, scale assistance enabled for the lead part, and gentle timing correction. Test these defaults through playing.

Changing Assist affects new playing and recording. It must not rewrite notes or timing already recorded. Any later operation that transforms a recorded phrase should be explicit and undoable.

**Swing is a separate groove control.** Zero means straight timing. Increasing it delays alternating subdivisions to change the rhythmic feel. It can change playback timing without permanently rewriting recorded events; its useful range and per-part behavior need auditioning.

Additional assistance to explore after the basic loop works: suggested next chords, chord strum, and an optional bassline that follows chord roots.

## Hands-on synthesis

The first sound page exposes a few meaningful controls such as brightness, movement, grit, and space. Labels and useful parameter ranges should fit the selected sound.

Deeper editing exposes oscillator waves and shape, envelopes, filters, and effects. Explore fuzz, chorus, delay, and reverb within the device's processing budget. Sound exploration is a core activity, not an advanced feature hidden behind the beginner workflow.

## First playable prototype

Build one complete, small experience before expanding the sound library:

- One curated palette with drums, bass, chords, and lead.
- A beat that starts quickly and can be edited by playing drum hits.
- A short chord loop, bass recording, and live lead playing.
- The Assist menu, separate swing control, mute, and undo.
- A small set of useful sound-shaping controls.
- A browser version sharing instrument logic with the eventual device build.

Initial loops record note/control events. Live guitar or microphone audio recording and layered audio loops require separate investigation.

The prototype succeeds when the owner can make a groove, record a two-chord phrase, add bass, and enjoy exploring the sounds with little explanation. Evaluate the sounds by listening and the controls by playing.

## Technical foundation and validation

Research identified Felucca as the common firmware foundation for the supplied examples. The deeper comparison selected pinned Sloop as the initial technical baseline because it supplies three synth parts and a dedicated drum system. JamBox will have its own workflow and sound curation. The exact source pin, module boundaries and validation gates are in [the implementation plan](docs/IMPLEMENTATION_PLAN.md).

The browser emulator approach compiles shared C code into WebAssembly with simulated hardware. It supports sound and interface iteration. Device timing, USB, flash writes, and recovery require physical hardware validation.

Before distributing a build, establish memory and audio-processing budgets, versioned project formats, backups, supported firmware transitions, and a recovery route. The first hardware transition must account for the owner's currently installed Sloop version.

The inspected firmware code is GPL-3.0-only. Preserve corresponding-source distribution and applicable notices, and select sounds and visual assets with suitable licenses.

## Source references

- [Felucca](https://github.com/hugelton/Felucca): common firmware platform and build tools.
- [Ghoulbox](https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth): focused sound identity and shared-source browser emulator.
- [Sloop](https://github.com/isod89/sloop-fm1): groovebox and performance workflow; the owner's current firmware.
- [ChoralRoot](https://github.com/Quixotic7/ChoralRootFM1): chord-oriented interaction, emulator, and update-compatibility work.
- [Source-pinned research and validation evidence](docs/RESEARCH_EVIDENCE.md), retained in this repository with logs and REA artifacts.
