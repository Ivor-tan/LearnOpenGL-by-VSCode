#version 330 core
out vec4 FragColor;
uniform vec4 testColor;
void main()
{
    // FragColor = vec4(0.0, 0.3176, 1.0, 1.0);
    FragColor = testColor;
}