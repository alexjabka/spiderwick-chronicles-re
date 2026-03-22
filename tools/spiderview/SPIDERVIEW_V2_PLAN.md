# SpiderView v2 — Audio + Animation + Cutscene Playback Plan

## What We Know

### Audio System: DirectMusic + XACT (Microsoft middleware)
- `.seg` — DirectMusic Segments. Magic: `seg\0pc_\0`. Header has 44100Hz sample rate at +0x18, embedded names at +0x38. ~5000 files (us/ = voice lines, ww/ = music + SFX)
- `.bnk` — XACT Sound Banks. Magic: `bankpc_\0`. Named banks (e.g. `AUDIO_BANK_MASTRSFX`). ~490 files
- `.epc` — AudioEffects definitions (1 per level)
- VM functions: `sauPlayCue`, `sauPlaySegment`, `sauStopSound`, `sauPauseMusic`
- DBDB entries: soundbankdb (150 records), audiohookdb (29), audioduckingdb (7), adaptivemusicdb

### Animation System: Custom proprietary
- `.aniz` — Skeleton + animation list. `hier` (N bones × 192B = local/world/inverseBind 4x4 matrices), `skel` (39 GPU bones × 48B float4x3). Partially reversed
- `.adat` — Animation keyframe data. Compressed/quantized, channel descriptor table. UNREVERSED. ~320 files
- `.amap` — Animation mapping tables. UNREVERSED
- Vertex shader: boneMatrix[22] at c25, D3DCOLORtoUBYTE4 swizzle, 11 morph targets

### Cutscenes: Proprietary in-game cinematics
- `.brxb` — LoadedCinematic (typeHash 0x3072B942). UNREVERSED. 9 files
- StreamedCinematic (typeHash 0xEAB838D4). UNREVERSED

---

## Phase 1: Audio Playback (SEG files)
**Effort: Medium | Value: High** — 5000+ voice lines and music tracks playable

1. **Reverse SEG header** — Pin down: audio data offset, codec (PCM vs IMA-ADPCM), channel count, sample rate, entry table structure
2. **SEG parser in SpiderView** — Format handler: detect, info (name + duration + sampleRate), raw audio extraction
3. **Audio playback engine** — Use raylib InitAudioDevice/LoadSoundFromWave/PlaySound. Extract PCM from SEG, feed to raylib
4. **Audio player UI** — Play/Pause/Stop, seek bar, waveform vis, volume slider. List view for SEGs in a WAD
5. **BNK bank browser** — Parse bank TOC, list contained sound entries, play individual sounds

## Phase 2: Animation Playback (ANIZ + ADAT)
**Effort: High | Value: Very High** — Animating all game characters in the viewer

1. **Reverse ADAT decompressor** — Trace Kallis runtime animation decompression in IDA. Find function that takes compressed keyframes → per-bone transforms
2. **Animation system in SpiderView** — Apply bone transforms to NM40 mesh per frame. GPU skinning or CPU vertex transform
3. **Animation timeline UI** — Playback controls, frame scrubber, animation list, blending preview
4. **ANIZ ↔ ADAT ↔ NM40 binding** — Map animations to skeletons/meshes via AMAP or heuristics

## Phase 3: In-Game Cinematics (BRXB)
**Effort: Very High | Value: Medium** — Only 9 files, needs full format RE

1. **Reverse brxb format** — Likely contains: camera paths, animation triggers, audio cues, subtitle references (STTL)
2. **Cinematic player** — Orchestrate camera + character animations + audio + subtitles in SpiderView

---

## Start Order

Phase 1 (Audio) first:
- SEG header already partially readable from hex dumps
- raylib has built-in audio playback
- Immediate tangible result (hearing the game's audio)
- No IDA RE needed if format is straightforward PCM
