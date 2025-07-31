#define PI 3.1415926538

out vec4 fragColor;

void main() {
  // Normalized pixel coordinates (from 0 to 1)
  vec2 uv = gl_FragCoord.xy / iResolution.xy;
  float leftBall = distance(vec2(gl_FragCoord.x + 200.0 * cos(iTime),
                                 gl_FragCoord.y + 200.0 * sin(iTime)),
                            iMouse.xy);
  float rightBall = distance(vec2(gl_FragCoord.x + 200.0 * cos(iTime + PI),
                                  gl_FragCoord.y + 200.0 * sin(iTime + PI)),
                             iMouse.xy);

  vec3 cot = vec3(0.0, 0.0, 0.0);

  // Adding left ball
  cot += vec3(100.0 * cos(iTime), 100.0 * cos(iTime + PI / 2.0),
              100.0 * cos(iTime + PI)) *
         (1.0 / leftBall);

  // Adding right ball
  cot += vec3(100.0 * cos(iTime + PI), 100.0 * cos(iTime + PI / 2.0),
              100.0 * cos(iTime)) *
         (1.0 / rightBall);

  // Output to screen
  fragColor = vec4(cot, 1.0);
}
