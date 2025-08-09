void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  // Normalize coordinates to [0,1]
  vec2 uv = fragCoord / iResolution.xy;

  // Sample the video texture from channel 0
  vec4 videoColor = texture(iChannel0, uv);

  // Output the video directly
  fragColor = videoColor;
}
