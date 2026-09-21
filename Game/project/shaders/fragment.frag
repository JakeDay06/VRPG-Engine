#version 460
layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoords;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec4 fragPosLightSpace;


struct Light{
  vec3 position;
  vec3 color;
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 lightSpaceMatrix;
    vec3 viewPos;
    Light lights[3];
    int objLighting;
} ubo;

layout(binding = 1) uniform sampler2D tex;
layout(binding = 2) uniform sampler2DShadow shadowMap;

float calculateShadow(vec4 posLightSpace){
  vec3 projection = posLightSpace.xyz / posLightSpace.w;

  projection.xy = projection.xy * 0.5 + 0.5;
  if (projection.z > 1.0) return 1.0;

  return texture(shadowMap, vec3(projection.xy, projection.z));
}

void main() {
  vec3 viewPos = ubo.viewPos;

  float ambientLight = 0.5f;
 
  vec3 texColor = texture(tex, fragTexCoords).rgb;
  vec3 specColor = vec3(1.0f, 1.0f, 0.75f);
  
  vec3 lightOut = texColor;
  if (ubo.objLighting != 0){
    lightOut = lightOut * ambientLight;
  }

  for(int i = 0; i < 3 ; i++){
    Light light = ubo.lights[i];
    vec3 lightPos = light.position;
    vec3 lightColor = light.color;
 
    vec3 lightDir = normalize(lightPos);
    
    float attenuation = 1.0;

    if (i != 0) {
      float dist = length(lightPos - fragPos);
      attenuation = 1.0 / (1.0 + 1.0 * dist + 2.5 * dist * dist);
    }
    
    if (ubo.objLighting >= 1) {
      // Diffuse
      vec3 norm = normalize(fragNormal);
      float diff = max(dot(norm, lightDir), 0.0);
      /*
      if (diff > 0.75)
          diff = 1.0;
      else if (diff > 0.5)
          diff = 0.7;
      else if (diff > 0.25)
          diff = 0.4;
      else
          diff = 0.15;
      */
      vec3 diffuse = diff * texColor * lightColor;

      vec3 specular = vec3(0.0, 0.0, 0.0);
      if (ubo.objLighting == 2){
        // Specular
        vec3 viewDir = normalize(viewPos - fragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        /*
        if (spec > 0.5)
            spec = 1.0;
        else
            spec = 0.0;
        */
        specular = spec * specColor * lightColor; 
      }

      lightOut += (diffuse + specular) * attenuation;
    }

    

    if(i == 0){
      lightOut = lightOut * (calculateShadow(fragPosLightSpace) + ambientLight);
    }

  }
  
  outColor = vec4(lightOut, 1.0f);
}

