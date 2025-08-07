wlsbg(1)

# NAME

wlsbg - Wayland Shader Background

# SYNOPSIS

*wlsbg* <options...> [OUTPUT] SHADER.frag

Displays a shader on specified outputs of your Wayland session with support for complex buffer pipelines.

# OPTIONS

*-h, --help*
	Show help message and quit.

*-v, --version*
	Show the version number and quit.

*-f, --fps* <number>
	Max FPS (Frames per second) limit of the shader.
	If unspecified then the max fps will default to 60.

*-x, --scale* <number>
	Resolution scale of the shader, setting this to a lower number will improve performance.
	If unspecified then the scale will default to 1x.

*-l, --layer* <layer>
	Set the layer to display on, valid types are: _background_, _bottom_, _top_, or _overlay_.
	Default is _background_.

*-s, --shared-shader* <path>
	Path to a common shader file containing shared functions/definitions.

*-[0-9], --channel[0-9]* <resource>
	Set the input for a specified channel (0-9) using shader buffer syntax:
	- `t:<path>`: Load texture from image file
	- `b:<path>`: Create shader buffer from fragment shader
	- `(resources...)`: Nested buffer definitions
	- `tName:<path>`, `bName:<path>`, etc.: Named resources, parsed from left to right

# REQUIRED ARGUMENTS

*[OUTPUT]*
	Select an output to configure. Subsequent appearance options will only
	apply to this output. The special value '_\*_' selects all outputs.

*[SHADER PATH]*
	Path to the main fragment shader that will be displayed to the output.

# UNIFORMS

*Shaders support these uniforms:*
	- _iResolution_            = vec3: (width, height, aspect ratio)
	- _iTime_                  = float: elapsed time (seconds)
	- _iTimeDelta_             = float: time since last frame
	- _iFrame_                 = int: frame count
	- _iFrameRate_             = float: estimated FPS
	- _iMouse_                 = vec4: mouse state (see below)
	- _iMousePos_              = vec2: real-time mouse position
	- _iDate_                  = vec4: (year, month, day, seconds)
	- _iChannel0..9_           = samplerXX: input channels
	- _iChannelResolution[10]_ = vec3[10]: channel resolutions

*About iMouse:*
	- _iMouse.xy_              = Last mouse down position
	- _abs(iMouse.zw)_         = Mouse position during last button click
	- _sign(iMouse.z)_         = Whether mouse button is down
	- _sign(iMouse.w)_         = Whether mouse button is clicked

# SHADER BUFFER SYNTAX

Channels support a powerful syntax for creating complex rendering pipelines:

*Basic resources:*
	- t:image.png          ; Texture from image file
	- b:buffer.frag        ; Shader buffer from fragment shader
	- bBackground:bg.frag  ; Named buffer resource

*Nested buffers:*
	"(t:tex1.jpg b:effect1.frag b:output.frag)"  ; Pipe texture through effect shader

	Last resource will be the output resource for the nested buffer, resources before that will
	be sequentially assigned as channels starting from 0 to 9, any more will cause an error.

*Multi-input buffers:*
	"(t:tex1.jpg b:effect1.frag (t:tex2.jpg b:effect2.frag) b:output.frag)"

*Channel references:*
	"wlsbg -0 tBackground:bg.png -1 (tBackground b:combine.frag b:effect.frag) '\*' image.frag"

# SPECIAL CHANNEL INPUTS

Channels can also accept special pre-named resources.

*tKeyboard*
	This resource is a 256x3 pixel texture encoding the current state of the keyboard.
	The states are represented as such:
		- First 256  = Key down [0 or 255]
		- Second 256 = Key just pressed [0 or 255]
		- Third 256  = Key toggled [0 or 255]

Look in the examples directory for more information on how to use these special resources.

# EXAMPLES

See example shaders by visiting the Github at <https://github.com/Sublimeful/wlsbg/tree/master/examples>.

# AUTHORS

Maintained by Jian Qiang Wu <Qiangwu2000@gmail.com>. For more information about wlsbg development, see
<https://github.com/Sublimeful/wlsbg>.
