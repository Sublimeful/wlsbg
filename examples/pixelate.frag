// Simple pixelation effect
precision mediump float;

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  float pixelSize = 5.0;
  vec2 uv = floor(fragCoord / pixelSize) * pixelSize;
  uv /= iResolution.xy;
  fragColor = texture(iChannel0, uv);
}
