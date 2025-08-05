/* bufferB: composite1 */
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec3 c = texture(iChannel0, uv).rgb; 
    vec2 p = 1.0 / iResolution.xy;
    float offset = 1.0;
    vec3 blur = c * 0.227027;
    blur += texture(iChannel0, uv + vec2(0.0, p.y * offset)).rgb * 0.1945946;
    blur += texture(iChannel0, uv - vec2(0.0, p.y * offset)).rgb * 0.1945946;
    blur += texture(iChannel0, uv + vec2(0.0, 2.0 * p.y * offset)).rgb * 0.1216216;
    blur += texture(iChannel0, uv - vec2(0.0, 2.0 * p.y * offset)).rgb * 0.1216216;
    blur += texture(iChannel0, uv + vec2(0.0, 3.0 * p.y * offset)).rgb * 0.054054;
    blur += texture(iChannel0, uv - vec2(0.0, 3.0 * p.y * offset)).rgb * 0.054054;
    blur += texture(iChannel0, uv + vec2(0.0, 4.0 * p.y * offset)).rgb * 0.016216;
    blur += texture(iChannel0, uv - vec2(0.0, 4.0 * p.y * offset)).rgb * 0.016216;
    fragColor = vec4(blur, 1.0);
}
