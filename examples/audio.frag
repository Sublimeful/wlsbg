void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  // Normalize coordinates to [0,1]
  vec2 uv = fragCoord / iResolution.xy;

  // Sample the audio frequencies from channel 0
  float frequency = texture(iChannel0, uv).x;

  // Output the frequencies directly
  fragColor = vec4(frequency, 0, 0, 1);
}
