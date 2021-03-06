#version 330 core
out vec4 FragColor;

// in vec3 ourColor;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform sampler2D texture2;

void main()
{
    // FragColor = texture(ourTexture, TexCoord);
    // FragColor = texture(ourTexture, TexCoord) * vec4(ourColor, 1.0);

    // linearly interpolate between both textures (80% container, 20% awesomeface)
      FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.35);
    //翻转第二张图
    // FragColor = mix(texture(texture1, TexCoord), texture(texture2, vec2(1.0f - TexCoord.x, TexCoord.y)), 0.2);

}