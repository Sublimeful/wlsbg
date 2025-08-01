wlsbg(1)

# NAME

wlsbg - Wayland Shader Background

# SYNOPSIS

*wlsbg* <options...> [OUTPUT] SHADER.frag

Displays a shader on specified outputs of your Wayland session.

# OPTIONS

*-h, --help*
	Show help message and quit.

*-v, --version*
	Show the version number and quit.

*-f, --fps* <number>
	Max FPS (Frames per second) limit of the shader.
	If unspecified then the max fps will default to 60.

*-l, --layer* <layer>
	Set the layer to display on, valid types are: _background_, _bottom_, _top_, or _overlay_.
	Default is _background_.

*-[0-3], --channel[0-3]* <path>
	Set the texture for a specified channel.
	This texture can be used in your shader by referencing the uniform.

# REQUIRED ARGUMENTS

*[OUTPUT]*
	Select an output to configure. Subsequent appearance options will only
	apply to this output. The special value '_\*_' selects all outputs.
	Note: The single quotes around the asterisk are there to
	prevent the shell from expanding the wildcard \* symbol.

*[SHADER PATH]*
	Specifies the path of a GLSL shader that will be displayed to the output.++
	Supported shader uniforms:++
		_iResolution_           - vec3, (displayWidth, displayHeight, aspectRatio).++
		_iTime_                 - float, time elapsed (in seconds).++
		_iTimeDelta_            - float, time since last frame (in seconds).++
    	_iFrame_                - int, frame count since start.++
		_iFrameRate_            - float, estimated frames per second.++
		_iMouse_                - mouse.xy      = mouse position during last button down.++
				              abs(mouse.zw) = mouse position during last button click.++
				              sign(mouze.z) = button is down.++
			                   sign(mouze.w) = button is clicked.++
		_iMousePos_             - vec2, real time position of the mouse.++
    	_iDate_                 - vec4, (year, month, day, time in seconds).++
		_iChannel0..3_          - samplerXX, input channel. XX = 2D/Cube.++
    	_iChannelResolution[4]_ - vec3 x 4, resolution of input textures, (width, height, aspectRatio).++
See example shaders by visiting the Github at <https://github.com/Sublimeful/wlsbg/tree/master/examples>.

# AUTHORS

Maintained by Jian Qiang Wu <Qiangwu2000@gmail.com>. For more information about wlsbg development, see
<https://github.com/Sublimeful/wlsbg>.
