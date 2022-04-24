#version 430

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

uniform layout(binding = 0) sampler2D s_Albedo;
uniform layout(binding = 1) sampler2D s_NormalsMetallic;
uniform layout(binding = 2) sampler2D s_DiffuseAccumulation;
uniform layout(binding = 3) sampler2D s_SpecularAccumulation;
uniform layout(binding = 4) sampler2D s_Emissive;
uniform vec3 s_Ambient;


#include "../fragments/frame_uniforms.glsl"

#include "../fragments/multiple_point_lights.glsl"


// Our color correction 3d texture
uniform layout (binding=14) sampler3D s_ColorCorrection;

// Function for applying color correction
vec3 ColorCorrect(vec3 inputColor) {
    // If our color correction flag is set, we perform the color lookup
    if (IsFlagSet(FLAG_ENABLE_COLOR_GRADING_CUSTOM)||IsFlagSet(FLAG_ENABLE_COLOR_GRADING_WARM)||IsFlagSet(FLAG_ENABLE_COLOR_GRADING_COOL) ) {
        return texture(s_ColorCorrection, inputColor).rgb;
    }
    // Otherwise just return the input
    else {
        return inputColor;
    }
}


void main() {
    vec3 albedo = texture(s_Albedo, inUV).rgb;
    vec3 diffuse = texture(s_DiffuseAccumulation, inUV).rgb;
    vec3 specular = texture(s_SpecularAccumulation, inUV).rgb;
    vec4 emissive = texture(s_Emissive, inUV);

    vec3 ambient = vec3(0.3, 0.3, 0.3);
    
    int lightCheck = 0;
    
    float colorAdjust = 1;

    if(IsFlagSet(FLAG_DISABLE_AMBIENT_LIGHTING))
    {
            ambient *= 0;

            //check if no light elements are on
            lightCheck = 0;
            colorAdjust = 1;
           
    }

    if(!IsFlagSet(FLAG_DISABLE_DIFFUSE_LIGHTING))
    {
            diffuse *=  0;

            //check if no light elements are on
            lightCheck = 0;
            colorAdjust = 1;
           
    }


    if(!IsFlagSet(FLAG_DISABLE_SPECULAR_LIGHTING))
    {
            specular *= 0;

            //check if no light elements are on
            lightCheck = 0;
            colorAdjust = 1;
    }

	outColor = vec4(ColorCorrect(albedo * (ambient + diffuse + specular + vec3(lightCheck) + (emissive.rgb * emissive.a)) * colorAdjust), 1.0);
}