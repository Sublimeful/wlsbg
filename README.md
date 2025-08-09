# wlsbg (Wayland Shader Background)

wlsbg is an advanced wallpaper utility for Wayland compositors that supports complex shader pipelines with up to 10 input channels. It's compatible with any Wayland compositor implementing wlr-layer-shell protocol and `wl_output` version 4.

## Features

- Multi-output support
- Shader output support
- Interaction uniforms
- 10-channel input pipeline
- Nested shader buffers
- Shared shader includes
- Video and Audio input

## Installation

### Dependencies

- meson\*
- wayland
- wayland-protocols\*
- egl-mesa
- mpv
- [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (man pages)\*
- git (version info)\*
- stb_image (bundled)

_\* Compile-time dependencies_

### Compiling from Source

```bash
meson setup build/
ninja -C build/
sudo ninja -C build/ install
```

Or you can just run the convenient setup, build and install scripts.

```bash
./setup.sh && ./build.sh && ./install.sh
```

## Documentation

See `man wlsbg` or [online documentation](https://github.com/Sublimeful/wlsbg/wiki) for advanced usage.

## Contributing

Submit issues and PRs at [GitHub repository](https://github.com/Sublimeful/wlsbg)
