#version 330 core

struct Material{
    sampler2D diffuse;
    sampler2D specular;
    sampler2D emission;
    float shininess;
};
struct Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 viewPos;
uniform Material material;
uniform Light light;
uniform float matrixlight;
uniform float matrixmove;


void main()
{
    // ambient
//  vec3 ambient = light.ambient * texture(material.diffuse, TexCoords).rgb;
  	vec3 ambient  = light.ambient  * vec3(texture(material.diffuse, TexCoords));
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
//     vec3 diffuse = light.diffuse * diff * texture(material.diffuse, TexCoords).rgb;  
    vec3 diffuse  = light.diffuse  * diff * vec3(texture(material.diffuse, TexCoords));  
    
    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
//  vec3 specular = light.specular * (spec * material.specular);  
    vec3 specular = light.specular * spec * vec3(texture(material.specular, TexCoords));
    
    //emission
    vec3 emission = matrixlight * texture(material.emission,vec2(TexCoords.x*2,TexCoords.y*2+matrixmove)).rgb;

    vec3 result = ambient + diffuse + specular + emission;
    FragColor = vec4(result,1.0);
}       