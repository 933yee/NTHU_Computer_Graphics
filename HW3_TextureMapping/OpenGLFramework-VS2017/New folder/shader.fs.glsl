#version 330

out vec4 fragColor;

in vec3 vertex_pos;
in vec3 vertex_color;
in vec3 vertex_normal;
in vec2 texCoord;

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

// [TODO] passing texture from main.cpp
// Hint: sampler2D
uniform sampler2D DiffuseTexture;
uniform int IsEye;
uniform float OffsetX;
uniform float OffsetY;

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

void main() {
	if(PerPixelLightingMode == 1){
		vec3 tmp_color;
		if (LightMode == 0) tmp_color = DirectionalLight(vertex_pos, vertex_normal);
		else if (LightMode == 1) tmp_color = PositionLight(vertex_pos, vertex_normal);
		else tmp_color = SpotLight(vertex_pos, vertex_normal);

		 fragColor = vec4(tmp_color, 1.0f);
	}
	else{
		 fragColor = vec4(vertex_color, 1.0f);
	}

	// [TODO] sampleing from texture
	// Hint: texture
	if (IsEye == 0)
        fragColor *= texture(DiffuseTexture, texCoord);
    else
        fragColor *= texture(DiffuseTexture, texCoord + vec2(OffsetX, OffsetY));
}
