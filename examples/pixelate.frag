// Simple pixelation effect
precision mediump float;

void mainImage(out vec4 fragColor, in vec2 vTexCoord) {
  float pixelSize = 5.0;
  vec2 uv = floor(gl_FragCoord.xy / pixelSize) * pixelSize;
  uv /= iResolution.xy;
  fragColor = texture(iChannel0, uv);
}
