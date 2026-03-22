#include "world_shader.h"
#include "rlgl.h"

bool WorldShader::Load() {
    shader = LoadShaderFromMemory(
        "#version 330\n"
        "in vec3 vertexPosition;\n"
        "in vec2 vertexTexCoord;\n"
        "in vec4 vertexColor;\n"
        "uniform mat4 mvp;\n"
        "out vec2 fragTexCoord;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragTexCoord = vertexTexCoord;\n"
        "    fragColor = vertexColor;\n"
        "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
        "}\n",
        "#version 330\n"
        "in vec2 fragTexCoord;\n"
        "in vec4 fragColor;\n"
        "uniform sampler2D texture0;\n"
        "uniform vec4 colDiffuse;\n"
        "uniform float uLightBoost;\n"
        "uniform float uAmbientLevel;\n"
        "uniform int uEnableTextures;\n"
        "uniform int uEnableVertColor;\n"
        "uniform int uEnableLighting;\n"
        "uniform float uAlphaClip;\n"
        "uniform int uNormalLighting;\n"
        "uniform int uEnableDirLight;\n"
        "uniform vec3 uLightDir;\n"
        "uniform float uDirLightIntensity;\n"
        "out vec4 finalColor;\n"
        "void main() {\n"
        "    vec4 tex = (uEnableTextures != 0) ? texture(texture0, fragTexCoord) : vec4(1.0);\n"
        "    vec3 lit;\n"
        "    float alpha;\n"
        "    if (uNormalLighting != 0) {\n"
        "        vec3 normal = normalize(fragColor.rgb * 2.0 - 1.0);\n"
        "        float NdotL = 0.0;\n"
        "        if (uEnableDirLight != 0)\n"
        "            NdotL = max(dot(normal, normalize(uLightDir)), 0.0) * uDirLightIntensity;\n"
        "        lit = vec3(uAmbientLevel + NdotL);\n"
        "        alpha = tex.a;\n"
        "    } else {\n"
        "        vec3 vc = (uEnableVertColor != 0) ? fragColor.rgb : vec3(1.0);\n"
        "        lit = (uEnableLighting != 0) ? vc * uLightBoost + vec3(uAmbientLevel) : vc;\n"
        "        alpha = tex.a * fragColor.a;\n"
        "    }\n"
        "    if (alpha < uAlphaClip) discard;\n"
        "    finalColor = vec4(tex.rgb * lit * colDiffuse.rgb, alpha);\n"
        "}\n"
    );
    locLightBoost   = GetShaderLocation(shader, "uLightBoost");
    locAmbientLevel = GetShaderLocation(shader, "uAmbientLevel");
    locEnableTex    = GetShaderLocation(shader, "uEnableTextures");
    locEnableVC     = GetShaderLocation(shader, "uEnableVertColor");
    locEnableLight  = GetShaderLocation(shader, "uEnableLighting");
    locAlphaClip    = GetShaderLocation(shader, "uAlphaClip");
    locEnableDirL   = GetShaderLocation(shader, "uEnableDirLight");
    locLightDir     = GetShaderLocation(shader, "uLightDir");
    locDirLightInt  = GetShaderLocation(shader, "uDirLightIntensity");
    locEnableFog    = GetShaderLocation(shader, "uEnableFog");
    locFogColor     = GetShaderLocation(shader, "uFogColor");
    locFogStart     = GetShaderLocation(shader, "uFogStart");
    locFogEnd       = GetShaderLocation(shader, "uFogEnd");
    locCameraPos    = GetShaderLocation(shader, "uCameraPos");
    locNormalLighting = GetShaderLocation(shader, "uNormalLighting");
    return shader.id > 0;
}

void WorldShader::Unload() { UnloadShader(shader); }

void WorldShader::Apply(const RenderSettings& s, const Camera3D& cam) {
    SetShaderValue(shader, locLightBoost, &s.lightBoost, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, locAmbientLevel, &s.ambientLevel, SHADER_UNIFORM_FLOAT);
    int iTex = s.layerTextures ? 1 : 0;
    int iVC  = s.layerVertColor ? 1 : 0;
    int iLit = s.layerLighting ? 1 : 0;
    SetShaderValue(shader, locEnableTex, &iTex, SHADER_UNIFORM_INT);
    SetShaderValue(shader, locEnableVC, &iVC, SHADER_UNIFORM_INT);
    SetShaderValue(shader, locEnableLight, &iLit, SHADER_UNIFORM_INT);
    float alphaClip = 15.0f / 255.0f;
    SetShaderValue(shader, locAlphaClip, &alphaClip, SHADER_UNIFORM_FLOAT);

    int iDirL = s.dirLightEnable ? 1 : 0;
    SetShaderValue(shader, locEnableDirL, &iDirL, SHADER_UNIFORM_INT);
    float ld[3] = { cosf(s.dirLightPitch)*sinf(s.dirLightYaw), sinf(s.dirLightPitch), cosf(s.dirLightPitch)*cosf(s.dirLightYaw) };
    SetShaderValue(shader, locLightDir, ld, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, locDirLightInt, &s.dirLightIntensity, SHADER_UNIFORM_FLOAT);

    int iFog = s.fogEnable ? 1 : 0;
    SetShaderValue(shader, locEnableFog, &iFog, SHADER_UNIFORM_INT);
    SetShaderValue(shader, locFogColor, (float*)s.fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, locFogStart, &s.fogStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, locFogEnd, &s.fogEnd, SHADER_UNIFORM_FLOAT);
    float cp[3] = {cam.position.x, cam.position.y, cam.position.z};
    SetShaderValue(shader, locCameraPos, cp, SHADER_UNIFORM_VEC3);

    int iNormLit = s.normalLighting ? 1 : 0;
    SetShaderValue(shader, locNormalLighting, &iNormLit, SHADER_UNIFORM_INT);
}

void WorldShader::Begin() { BeginShaderMode(shader); }
void WorldShader::End()   { EndShaderMode(); }
