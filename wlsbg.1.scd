wlsbg(1)

# NAME

wlsbg - Wayland shader background

# SYNOPSIS

*wlsbg* [options...]

Displays a background image and an optional shader on all outputs of your Wayland session.

Without an output specified, appearance options apply to all outputs.
Per-output appearance options can be set by passing _-o, --output_ followed by
these options.

# OPTIONS

*-c, --color* <[#]rrggbb>
	Set the background color.

*-f, --fps* <int>
	Max FPS (Frames per second) limit of the shader.
	If unspecified then the max fps will default to 60.

*-h, --help*
	Show help message and quit.

*-i, --image* <path>
	Set the background image.

*-l, --layer* <layer>
	Set the layer to display on, valid types are: _background_, _bottom_, _top_, or _overlay_.
	Default is _background_.

*-m, --mode* <mode>
	Scaling mode for images: _stretch_, _fill_, _fit_, _center_, or _tile_.
	Default is _stretch_. Use the additional mode _solid\_color_ to display
	only the background color, even if a background image is specified.

*-o, --output* <name>
	Select an output to configure. Subsequent appearance options will only
	apply to this output. The special value _\*_ selects all outputs.

*-s, --shader* <path>
	Apply GLSL shader to wallpaper, must have background image set. Supported uniforms:++
	_iTexture_ - sampler2D, this is what the image texture is binded to.++
	_iResolution_ - vec3(displayWidth, displayHeight, aspectRatio).++
	_iTime_ - float, time in seconds.++
	_iMouse_ - mouse.xy = mouse position during last button down.++
		abs(mouse.zw) = mouse position during last button click.++
		sign(mouze.z) = button is down.++
		sign(mouze.w) = button is clicked.++
	_iMousePos_ - vec2, real time position of the mouse.++
There is also the input vTexCoord, which maps to the texture coordinates of the input image. See example shaders by visiting the Github at <https://github.com/Sublimeful/wlsbg/tree/master/examples>.

*-v, --version*
	Show the version number and quit.

# AUTHORS

Maintained by Jian Qiang Wu <Qiangwu2000@gmail.com>. For more information about wlsbg development, see
<https://github.com/Sublimeful/wlsbg>.

Original credits to Simon Ser <contact@emersion.fr>, who is assisted by other open
source contributors. For more information about swaybg development, see
<https://github.com/swaywm/swaybg>.
