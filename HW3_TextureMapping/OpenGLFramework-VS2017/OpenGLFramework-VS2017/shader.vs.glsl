#version 330

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;

out vec3 vertex_pos;
out vec3 vertex_color;
out vec3 vertex_normal;
out vec2 texCoord;

uniform mat4 um4p;	
uniform mat4 um4v;
uniform mat4 um4m;

float PI = 3.1415926535;

uniform vec3 CameraPosition;
uniform vec3 LightPosition;
uniform vec3 LightDirection;
uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 Ks;
uniform vec3 Ambient;
uniform vec3 Diffuse;
uniform vec3 Specular;
uniform float Shininess;
uniform float Constant;
uniform float Linear;
uniform float Quadratic;
uniform float Exponent;
uniform float Cutoff;
uniform int LightMode;
uniform int PerPixelLightingMode;

vec3 DirectionalLight(vec3 vertexPosition, vec3 vertexNormal)
{
    vec3 ambient = Ambient * Ka;

	vec3 L = normalize(LightPosition);
    vec3 N = normalize(vertexNormal);
    vec3 diffuse = max(dot(N, L), 0.0f) * Diffuse * Kd;

    vec3 V = normalize(CameraPosition - vertexPosition);
    vec3 H = normalize(L + V);

    vec3 specular = pow(max(dot(N, H), 0.0f), Shininess) * Specular * Ks;

    return ambient + diffuse + specular;
}

vec3 PositionLight(vec3 vertexPosition, vec3 vertexNormal)
{
	vec3 ambient = Ambient * Ka;

    vec3 L = normalize(LightPosition - vertexPosition);
	vec3 N = normalize(vertexNormal);
    vec3 diffuse = max(dot(N, L), 0.0f) * Diffuse * Kd;

    vec3 V = normalize(CameraPosition - vertexPosition);
    vec3 H = normalize(L + V);
    vec3 specular = pow(max(dot(N, H), 0.0f), Shininess) * Specular * Ks;

    float dist = length(LightPosition - vertexPosition);
    float attenuation = Constant + Linear * dist + Quadratic * pow(dist, 2.0f);
    float f_att = min(1.0f / attenuation, 1.0f);

    return ambient + f_att * (diffuse + specular);
}

vec3 SpotLight(vec3 vertexPosition, vec3 vertexNormal)
{
    vec3 ambient = Ambient * Ka;

    vec3 L = normalize(LightPosition - vertexPosition);
    vec3 N = normalize(vertexNormal);
    vec3 diffuse = max(dot(N, L), 0.0f) * Diffuse * Kd;

    vec3 V = normalize(CameraPosition - vertexPosition);
    vec3 H = normalize(L + V);
    vec3 specular = pow(max(dot(N, H), 0.0f), Shininess) * Specular * Ks;

    float dist = length(LightPosition - vertexPosition);
    float attenuation = Constant + Linear * dist + Quadratic * pow(dist, 2.0f);
    float f_att = min(1.0f / attenuation, 1.0f);

    vec3 v = normalize(vertexPosition - LightPosition);
    vec3 d = normalize(LightDirection);
    float v_d = dot(v, d);
    float SpotEffect = 0;
    if (v_d > cos(Cutoff * PI / 180.0f))
        SpotEffect = pow(max(v_d, 0.0f), Exponent);

    return ambient + SpotEffect * f_att * (diffuse + specular);
}

// [TODO] passing uniform variable for texture coordinate offset

void main() 
{
	// [TODO]
	gl_Position = um4p * um4v * um4m * vec4(aPos, 1.0);
	vertex_pos = vec3(um4m * vec4(aPos, 1.0f));
    vertex_normal = mat3(transpose(inverse(um4m))) * aNormal;

	if (LightMode == 0)
        vertex_color = DirectionalLight(vertex_pos, vertex_normal);
    else if (LightMode == 1)
        vertex_color = PositionLight(vertex_pos, vertex_normal);
    else
        vertex_color = SpotLight(vertex_pos, vertex_normal);

	texCoord = aTexCoord;
}
