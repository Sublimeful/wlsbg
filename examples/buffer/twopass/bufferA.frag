/* bufferA: composite0 */
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec3 c = texture(iChannel0, uv).rgb; 
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float t = 0.8;
    float softEdge = 0.1;
    float weight = smoothstep(t, t + softEdge, luma);
    c *= weight;
    vec2 p = 1.0 / iResolution.xy;
    float offset = 1.0;
    vec3 blur = c * 0.227027;
    blur += texture(iChannel0, uv + vec2(p.x * offset, 0.0)).rgb * 0.1945946;
    blur += texture(iChannel0, uv - vec2(p.x * offset, 0.0)).rgb * 0.1945946;
    blur += texture(iChannel0, uv + vec2(p.x * 2.0 * offset, 0.0)).rgb * 0.1216216;
    blur += texture(iChannel0, uv - vec2(p.x * 2.0 * offset, 0.0)).rgb * 0.1216216;
    blur += texture(iChannel0, uv + vec2(p.x * 3.0 * offset, 0.0)).rgb * 0.054054;
    blur += texture(iChannel0, uv - vec2(p.x * 3.0 * offset, 0.0)).rgb * 0.054054;
    blur += texture(iChannel0, uv + vec2(p.x * 4.0 * offset, 0.0)).rgb * 0.016216;
    blur += texture(iChannel0, uv - vec2(p.x * 4.0 * offset, 0.0)).rgb * 0.016216;
    fragColor = vec4(blur, 1.0);
}
