#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform float uWindowCenter = 0.5;   // центр окна (в единицах текстуры)
uniform float uWindowWidth  = 1.0;   // ширина окна (в тех же единицах)

void main()
{
    vec4 originalColor = texture(ourTexture, TexCoord);

    float low  = uWindowCenter - uWindowWidth / 2.0;
    float high = uWindowCenter + uWindowWidth / 2.0;

    // Избегаем деления на ноль
    float range = max(high - low, 1e-6);

    vec3 adjustedColor = (originalColor.rgb - low) / range;
    adjustedColor = clamp(adjustedColor, 0.0, 1.0);

    FragColor = vec4(adjustedColor, originalColor.a);
}