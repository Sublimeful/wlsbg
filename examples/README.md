# wlsbg Examples

## Basic Usage

```bash
# Simple shader
wlsbg '*' examples/retro.frag

# Shader with texture input
wlsbg -0 t:examples/kiki.jpg '*' examples/pixelate.frag

# Shader with video input
wlsbg -0 v:examples/buck.mp4 '*' examples/video.frag

# Shader with audio input
wlsbg -0 a:examples/audio.mp3 '*' examples/audio.frag
```

## Interactive Examples

```bash
# Mouse-controlled shader (click and drag)
wlsbg '*' examples/mouse.frag
```

```bash
# Keyboard-controlled shader (focus the background and tap on your keyboard)
wlsbg -0 tKeyboard '*' examples/keyboard.frag
```

## Buffer Pipelines

```bash
# One pass processing pipeline
wlsbg -0 bA:examples/buffer/simple/bufferA.frag \
      '*' \
      examples/buffer/simple/image.frag
```

```bash
# Two-stage processing pipeline
wlsbg -0 "(t:examples/kiki.jpg bA:examples/buffer/twopass/bufferA.frag)" \
      -1 "(bA bB:examples/buffer/twopass/bufferB.frag)" \
      -2 t:examples/kiki.jpg \
      '*' \
      examples/buffer/twopass/image.frag
```

```bash
# Multi-pass rendering with 4 buffers
wlsbg -0 "(bA:examples/buffer/multipass/bufferA.frag bB:examples/buffer/multipass/bufferB.frag bB)" \
      -1 "(bB bC:examples/buffer/multipass/bufferC.frag bC)" \
      -2 "(bC bD:examples/buffer/multipass/bufferD.frag)" \
      -3 t:examples/chars.png \
      -s examples/buffer/multipass/shared.frag \
      '*' \
      examples/buffer/multipass/image.frag

# This configuration:
# 1. Uses 4 nested shader buffers (A→B→C→D)
# 2. Loads a texture (chars.png) into channel 3
# 3. Uses shared.frag for shared functions
# 4. Runs image.frag as output shader
```

## Channel Types

| Syntax         | Description                 | Example                            |
| -------------- | --------------------------- | ---------------------------------- |
| `b:path`       | Shader buffer               | `b:effect.frag`                    |
| `t:path`       | Texture from image          | `t:image.png`                      |
| `v:path`       | Video input                 | `v:video.mp4`                      |
| `a:path`       | Audio input                 | `a:audio.mp3`                      |
| `<T>name:path` | Named resource              | `bBackground:bg.frag`              |
| `(res...)`     | Nested buffer configuration | `(t:tex.jpg b:fx.frag b:out.frag)` |
