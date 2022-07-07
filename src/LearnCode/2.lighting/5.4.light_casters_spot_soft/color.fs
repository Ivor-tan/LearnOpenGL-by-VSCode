#version 330 core

struct Material{
    sampler2D diffuse;
    sampler2D specular;
    sampler2D emission;
    float shininess;
};
struct Light{
    vec3 lightColor;
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    
    //衰减
    float constant;
    float linear;
    float quadratic;
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
    vec3 lightDir=normalize(light.position-FragPos);
    float theta=dot(lightDir,normalize(-light.direction));
    float epsilon=light.cutOff-light.outerCutOff;
    float intensity  = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    

        // ambient
        //  vec3 ambient = light.ambient * texture(material.diffuse, TexCoords).rgb;
        vec3 ambient=light.ambient*vec3(texture(material.diffuse,TexCoords));
        // diffuse
        vec3 norm=normalize(Normal);
        // vec3 lightDir = normalize(light.position - FragPos);
        // vec3 lightDir = normalize(-light.direction);//平行光源
        float diff=max(dot(norm,lightDir),0.);
        // vec3 diffuse = light.diffuse * diff * texture(material.diffuse, TexCoords).rgb;  //().rgb  等同于vec3()
        vec3 diffuse=light.diffuse*diff*texture(material.diffuse,TexCoords).rgb;
        
        // specular
        vec3 viewDir=normalize(viewPos-FragPos);
        vec3 reflectDir=reflect(-lightDir,norm);
        float spec=pow(max(dot(viewDir,reflectDir),0.),material.shininess);
        //  vec3 specular = light.specular * (spec * material.specular);
        vec3 specular=light.specular*spec*texture(material.specular,TexCoords).rgb;
        
        //emission
        // vec3 emission = matrixlight * texture(material.emission,vec2(TexCoords.x*2,TexCoords.y*2+matrixmove)).rgb;
        
        //衰减
        float distance=length(light.position-FragPos);
        float attenuation=1./(light.constant+light.linear*distance+light.quadratic*(distance*distance));
        
        ambient*=attenuation;
        diffuse*=attenuation;
        specular *= attenuation;

        // 将不对环境光做出影响，让它总是能有一点光
        diffuse  *= intensity;
        specular *= intensity;
        
        // vec3 result = ambient + diffuse + specular + emission;
        vec3 result = ambient + diffuse + specular;
        FragColor=vec4(result*light.lightColor,1.);
    
}