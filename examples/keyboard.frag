void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  // Normalize coordinates to [0,1]
  vec2 uv = fragCoord / iResolution.xy;

  // Get keyboard states
  float state = texelFetch(iChannel0, ivec2(int(uv.x * 256.0), 0), 0).r;
  float pressed = texelFetch(iChannel0, ivec2(int(uv.x * 256.0), 1), 0).r;
  float toggled = texelFetch(iChannel0, ivec2(int(uv.x * 256.0), 2), 0).r;

  vec3 color = vec3(0);
  if (state > 0.0 && uv.y > 0.66) {
    color = vec3(1, 0, 0);
  } else if (pressed > 0.0 && uv.y > 0.33) {
    color = vec3(0, 1, 0);
  } else if (toggled > 0.0 && uv.y < 0.33) {
    color = vec3(0, 0, 1);
  }

  fragColor = vec4(color, 1.0);
}
