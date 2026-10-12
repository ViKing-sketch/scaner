#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform float uWindowCenter = 0.5;   // центр окна, уже поделённый на 65535
uniform float uWindowWidth  = 1.0;   // ширина окна в тех же долях
uniform int uInvert = 0;
uniform int uMono = 1;

void main()
{
    vec4 originalColor = texture(ourTexture, TexCoord);
    float low  = uWindowCenter - uWindowWidth / 2.0;
    float high = uWindowCenter + uWindowWidth / 2.0;
    float range = max(high - low, 1e-6);

    vec3 adjustedColor;
    if (uMono != 0) {
        float shown = clamp((originalColor.r - low) / range, 0.0, 1.0);
        if (uInvert != 0) {
            shown = 1.0 - shown;
        }
        adjustedColor = vec3(shown);
    } else {
        adjustedColor = clamp((originalColor.rgb - low) / range, 0.0, 1.0);
        if (uInvert != 0) {
            adjustedColor = vec3(1.0) - adjustedColor;
        }
    }

    FragColor = vec4(adjustedColor, originalColor.a);
}
