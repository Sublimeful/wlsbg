/* image: final */
//FROM:
// Khronos PBR Neutral Tone Mapper

// https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral

// Input color is non-negative and resides in the Linear Rec. 709 color space.

// Output color is also Linear Rec. 709, but in the [0, 1] range.

vec3 neutral(vec3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;
    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;
    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;
    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, vec3(newPeak), g);
}

vec3 Applyneutral(float exposure, vec3 color) {

    color *= exposure;
  
    color = neutral(color);
  
    return clamp(color, 0.0, 1.0);
}
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec3 contrastMap = texture(iChannel2, uv).rgb; 
    vec3 bloom = texture(iChannel1, uv).rgb;      
    float exposure = 1.;
    float bloomStrength = .5; 
    vec3 toneMapScene = Applyneutral(exposure, contrastMap);
    vec3 finalColor = toneMapScene + bloom * bloomStrength;
    finalColor = clamp(finalColor, 0.0, 1.0);
    vec3 onlytoneMap = Applyneutral(exposure, contrastMap);
    float Hua_Dong_Ni_De_Shu_Biao = iMouse.z > 0.0 ? iMouse.x / iResolution.x : 0.5;
    if (uv.x < Hua_Dong_Ni_De_Shu_Biao) {
        fragColor = vec4(finalColor, 1.0);
    } else {
        fragColor = vec4(onlytoneMap, 1.0);
    }
}
