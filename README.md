# wlsbg (Wayland Shader Background)

wlsbg is a wallpaper utility for Wayland compositors. It is compatible with any Wayland
compositor which implements the wlr-layer-shell protocol and `wl_output` version 4.

See the man page, `wlsbg(1)`, for instructions on using wlsbg.

## Release Signatures

Releases are published [on GitHub](https://github.com/Sublimeful/wlsbg/releases).
wlsbg releases are managed independently of sway releases.

## Installation

### Compiling from Source

Install dependencies:

- meson \*
- wayland
- wayland-protocols \*
- egl-mesa
- [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (optional: man pages) \*
- git (optional: version information) \*

_\* Compile-time dep_

Run these commands:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install
