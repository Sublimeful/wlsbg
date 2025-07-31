// Simple pixelation effect
precision mediump float;
in vec2 vTexCoord;
out vec4 fragColor;

void main() {
  float pixelSize = 5.0;
  vec2 uv = floor(gl_FragCoord.xy / pixelSize) * pixelSize;
  uv /= iResolution.xy;
  fragColor = texture(iTexture, uv);
}
