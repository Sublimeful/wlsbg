/* This buffer handles the drawing part */

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  vec2 uv = fragCoord / R;
  vec2 m = (iMouse.xy - fragCoord) / R.y;

  float d = smoothstep(.06, 0., length(m)); // "Pen" intensity
  d *= step(0., iMouse.z);                  // Only draw on mouse press

  // Accumulate values and keep them in the [0-1] range
  vec3 col = texture(iChannel0, uv).rgb;
  col = clamp(col + d, 0., 1.);

  col *= step(iMouse.w, 0.); // Reset on new click

  fragColor = vec4(col, 1);
}
