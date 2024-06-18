#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Vectors.h"
#include "Matrices.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

using namespace std;

// Default window size
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
int window_width = WINDOW_WIDTH;
int window_height = WINDOW_HEIGHT;

bool mouse_pressed = false;
int starting_press_x = -1;
int starting_press_y = -1;

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	LightEdit = 3,
	ShininessEdit = 4,
};

enum LightMode
{
	DirectionalLight = 0,
	PositionLight = 1,
	SpotLight = 2,
};
LightMode cur_light_mode = DirectionalLight;

struct Uniform
{
	GLint iLocMVP;
	GLint M;

	GLint CameraPosition;
	// material
	GLint Ka;
	GLint Kd;
	GLint Ks;
	// light
	GLint Ambient;
	GLint Diffuse;
	GLint Specular;
	GLint Shininess;
	// Attenuation
	GLint Constant;
	GLint Linear;
	GLint Quadratic;
	// spot light
	GLint LightPosition;
	GLint LightDirection;
	GLint Exponent;
	GLint Cutoff;

	GLint LightMode;
	GLint PerPixelLightingMode;
};
Uniform uniform;

struct LightInformation
{
	Vector3 Position;
	Vector3 Direction;
	Vector3 Diffuse;
	Vector3 Ambient;
	Vector3 Specular;
	GLfloat Cutoff;
	GLfloat Constant;
	GLfloat Linear;
	GLfloat Quadratic;
	GLfloat Exponent;
};
GLfloat Shininess;
LightInformation lightInformation[3];

vector<string> filenames; // .obj filename list

struct PhongMaterial
{
	Vector3 Ka;
	Vector3 Kd;
	Vector3 Ks;
};

typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	PhongMaterial material;
	int indexCount;
	GLuint m_texture;
} Shape;

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form

	vector<Shape> shapes;
};
vector<model> models;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};
camera main_camera;

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};
project_setting proj;

TransMode cur_trans_mode = GeoTranslation;

Matrix4 view_matrix;
Matrix4 project_matrix;

int cur_idx = 0; // represent which model should be rendered now


static GLvoid Normalize(GLfloat v[3])
{
	GLfloat l;

	l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static GLvoid Cross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{

	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}


// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;


	mat = Matrix4(
		1, 0, 0, vec.x,
		0, 1, 0, vec.y,
		0, 0, 1, vec.z,
		0, 0, 0, 1
	);


	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;


	mat = Matrix4(
		vec.x, 0, 0, 0,
		0, vec.y, 0, 0,
		0, 0, vec.z, 0,
		0, 0, 0, 1
	);


	return mat;
}



// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;


	mat = Matrix4(
		1, 0, 0, 0,
		0, cos(val), -sin(val), 0,
		0, sin(val), cos(val), 0,
		0, 0, 0, 1
	);


	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;


	mat = Matrix4(
		cos(val), 0, sin(val), 0,
		0, 1, 0, 0,
		-sin(val), 0, cos(val), 0,
		0, 0, 0, 1
	);


	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;


	mat = Matrix4(
		cos(val), -sin(val), 0, 0,
		sin(val), cos(val), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);


	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera
void setViewingMatrix()
{
	Vector3 P1_P2 = main_camera.center - main_camera.position;
	Vector3 P1_P3 = main_camera.up_vector;

	GLfloat P1P2[3] = { P1_P2.x, P1_P2.y, P1_P2.z };
	GLfloat P1P3[3] = { P1_P3.x, P1_P3.y, P1_P3.z };

	GLfloat Rz[3] = { -P1_P2.x, -P1_P2.y, -P1_P2.z };
	Normalize(Rz);

	GLfloat Rx[3];
	Cross(P1P2, P1P3, Rx);
	Normalize(Rx);

	GLfloat Ry[3];
	Cross(Rz, Rx, Ry);


	// CG07 p.70
	GLfloat eye[3] = { main_camera.position.x, main_camera.position.y, main_camera.position.z };

	Matrix4 R = Matrix4(
		Rx[0], Rx[1], Rx[2], 0,
		Ry[0], Ry[1], Ry[2], 0,
		Rz[0], Rz[1], Rz[2], 0,
		0, 0, 0, 1
	);

	Matrix4 T = Matrix4(
		1, 0, 0, -eye[0],
		0, 1, 0, -eye[1],
		0, 0, 1, -eye[2],
		0, 0, 0, 1
	);

	view_matrix = R * T;
}

// [TODO] compute persepective projection matrix
void setPerspective()
{
	GLfloat Near_Add_Far = proj.nearClip + proj.farClip;
	GLfloat Near_Minus_Far = proj.nearClip - proj.farClip;
	GLfloat Near_Multiply_Far = proj.nearClip * proj.farClip;
	GLfloat f = 1 / tan(proj.fovy * 3.1415926535 / 180 / 2);

	if (proj.aspect / 2 >= 1) {
		project_matrix = Matrix4(
			f / proj.aspect * 2, 0, 0, 0,
			0, f, 0, 0,
			0, 0, Near_Add_Far / Near_Minus_Far, 2 * Near_Multiply_Far / Near_Minus_Far,
			0, 0, -1, 0
		);
	}
	else {
		project_matrix = Matrix4(
			f, 0, 0, 0,
			0, f * proj.aspect / 2, 0, 0,
			0, 0, Near_Add_Far / Near_Minus_Far, 2 * Near_Multiply_Far / Near_Minus_Far,
			0, 0, -1, 0
		);
	}
}

void setGLMatrix(GLfloat* glm, Matrix4& m) {
	glm[0] = m[0];  glm[4] = m[1];   glm[8] = m[2];    glm[12] = m[3];
	glm[1] = m[4];  glm[5] = m[5];   glm[9] = m[6];    glm[13] = m[7];
	glm[2] = m[8];  glm[6] = m[9];   glm[10] = m[10];   glm[14] = m[11];
	glm[3] = m[12];  glm[7] = m[13];  glm[11] = m[14];   glm[15] = m[15];
}

// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow* window, int width, int height)
{
	// [TODO] change your aspect ratio
	window_width = width;
	window_height = height;
	proj.aspect = (GLfloat)width / (GLfloat)height;
	setPerspective();
}

// Render function for display rendering
void RenderScene(void) {	
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 T, R, S;
	// [TODO] update translation, rotation and scaling
	T = translate(models[cur_idx].position);
	R = rotate(models[cur_idx].rotation);
	S = scaling(models[cur_idx].scale);

	Matrix4 MVP, M;
	GLfloat mvp[16], m[16];

	// [TODO] multiply all the matrix
	M = T * R * S;
	MVP = project_matrix * view_matrix * M;

	// row-major ---> column-major
	setGLMatrix(mvp, MVP);
	setGLMatrix(m, M);

	// use uniform to send mvp to vertex shader
	glUniformMatrix4fv(uniform.iLocMVP, 1, GL_FALSE, mvp);
	glUniformMatrix4fv(uniform.M, 1, GL_FALSE, m);
	glUniform3f(uniform.CameraPosition, main_camera.position.x, main_camera.position.y, main_camera.position.z);
	glUniform3f(uniform.LightPosition, lightInformation[cur_light_mode].Position.x, lightInformation[cur_light_mode].Position.y, lightInformation[cur_light_mode].Position.z);
	
	glUniform3f(uniform.Ambient, lightInformation[cur_light_mode].Ambient.x, lightInformation[cur_light_mode].Ambient.y, lightInformation[cur_light_mode].Ambient.z);
	glUniform3f(uniform.Diffuse, lightInformation[cur_light_mode].Diffuse.x, lightInformation[cur_light_mode].Diffuse.y, lightInformation[cur_light_mode].Diffuse.z);
	glUniform3f(uniform.Specular, lightInformation[cur_light_mode].Specular.x, lightInformation[cur_light_mode].Specular.y, lightInformation[cur_light_mode].Specular.z);
	glUniform1f(uniform.Shininess, Shininess);

	glUniform1f(uniform.Constant, lightInformation[cur_light_mode].Constant);
	glUniform1f(uniform.Linear, lightInformation[cur_light_mode].Linear);
	glUniform1f(uniform.Quadratic, lightInformation[cur_light_mode].Quadratic);

	glUniform3f(uniform.LightDirection, lightInformation[cur_light_mode].Direction.x, lightInformation[cur_light_mode].Direction.y, lightInformation[cur_light_mode].Direction.z);
	glUniform1f(uniform.Exponent, lightInformation[cur_light_mode].Exponent);
	glUniform1f(uniform.Cutoff, lightInformation[cur_light_mode].Cutoff);

	glUniform1i(uniform.LightMode, cur_light_mode);

	for (int i = 0; i < models[cur_idx].shapes.size(); i++) 
	{
		// set glViewport and draw twice ... 
		glUniform1i(uniform.PerPixelLightingMode, 0);
		glViewport(0, 0, window_width / 2, window_height);
		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);

		glUniform1i(uniform.PerPixelLightingMode, 1);
		glViewport(window_width / 2, 0, window_width / 2, window_height);
		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);

		glUniform3f(uniform.Ka, models[cur_idx].shapes[i].material.Ka.x, models[cur_idx].shapes[i].material.Ka.y, models[cur_idx].shapes[i].material.Ka.z);
		glUniform3f(uniform.Kd, models[cur_idx].shapes[i].material.Kd.x, models[cur_idx].shapes[i].material.Kd.y, models[cur_idx].shapes[i].material.Kd.z);
		glUniform3f(uniform.Ks, models[cur_idx].shapes[i].material.Ks.x, models[cur_idx].shapes[i].material.Ks.y, models[cur_idx].shapes[i].material.Ks.z);
	}
}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	if (action != GLFW_PRESS) return;
	switch (key) {
	case GLFW_KEY_Z: {
		++cur_idx;
		if (cur_idx >= models.size()) cur_idx = 0;
		break;
	}
	case GLFW_KEY_X: {
		--cur_idx;
		if (cur_idx < 0) cur_idx = models.size() - 1;
		break;
	}
	case GLFW_KEY_T: {
		cur_trans_mode = TransMode::GeoTranslation;
		break;
	}
	case GLFW_KEY_S: {
		cur_trans_mode = TransMode::GeoScaling;
		break;
	}
	case GLFW_KEY_R: {
		cur_trans_mode = TransMode::GeoRotation;
		break;
	}
	case GLFW_KEY_L: {
		switch (cur_light_mode)
		{
		case DirectionalLight:
			cur_light_mode = PositionLight;
			break;
		case PositionLight:
			cur_light_mode = SpotLight;
			break;
		case SpotLight:
			cur_light_mode = DirectionalLight;
			break;
		default:
			break;
		}
		break;
	}
	case GLFW_KEY_K: {
		cur_trans_mode = TransMode::LightEdit;
		break;
	}
	case GLFW_KEY_J: {
		cur_trans_mode = TransMode::ShininessEdit;
		break;
	}
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negtive
	yoffset *= 0.05;
	switch (cur_trans_mode)
	{
	case TransMode::GeoTranslation:
		models[cur_idx].position.z += yoffset;
		break;
	case TransMode::GeoScaling:
		models[cur_idx].scale.z += yoffset;
		break;
	case TransMode::GeoRotation:
		models[cur_idx].rotation.z += yoffset;
		break;
	case TransMode::LightEdit:
		switch (cur_light_mode)
		{
		case DirectionalLight:
		case PositionLight:
			lightInformation[cur_light_mode].Diffuse += Vector3(yoffset, yoffset, yoffset);
			break;
		case SpotLight:
			lightInformation[cur_light_mode].Cutoff += yoffset * 50.0f;
			if (lightInformation[cur_light_mode].Cutoff < -90.0f) lightInformation[cur_light_mode].Cutoff = -90.0f;
			if (lightInformation[cur_light_mode].Cutoff > 90.0f) lightInformation[cur_light_mode].Cutoff = 90.0f;
			break;
		default:
			break;
		}
		break;
	case TransMode::ShininessEdit:
		Shininess += yoffset * 50.0f;
		break;
	default:
		break;
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
	if (button == GLFW_MOUSE_BUTTON_LEFT)
		mouse_pressed = (action == GLFW_PRESS);
		
}

double xpos_prev, ypos_prev;

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// [TODO] cursor position callback function
	double xdiff = (xpos - xpos_prev) * 0.01;
	double ydiff = (ypos - ypos_prev) * 0.01;

	if (mouse_pressed) {
		switch (cur_trans_mode)
		{
		case TransMode::GeoTranslation:
			models[cur_idx].position.x += xdiff;
			models[cur_idx].position.y -= ydiff;
			break;
		case TransMode::GeoScaling:
			models[cur_idx].scale.x += xdiff;
			models[cur_idx].scale.y -= ydiff;
			break;
		case TransMode::GeoRotation:
			models[cur_idx].rotation.y -= xdiff;
			models[cur_idx].rotation.x -= ydiff;
			break;
		case TransMode::LightEdit:
			lightInformation[cur_light_mode].Position.x += xdiff ;
			lightInformation[cur_light_mode].Position.y -= ydiff ;
			break;
		default:
			break;
		}
	}

	xpos_prev = xpos;
	ypos_prev = ypos;
}

void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vs");
	fs = textFileRead("shader.fs");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	GLint success;
	char infoLog[1000];
	// compile vertex shader
	glCompileShader(v);
	// check for shader compile errors
	glGetShaderiv(v, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(v, 1000, NULL, infoLog);
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// create program object
	p = glCreateProgram();

	// attach shaders to program object
	glAttachShader(p,f);
	glAttachShader(p,v);

	// link program
	glLinkProgram(p);
	// check for linking errors
	glGetProgramiv(p, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(p, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n" << infoLog << std::endl;
	}

	glDeleteShader(v);
	glDeleteShader(f);

	// ·Ç³Æ¤@°ï uniform data
	uniform.iLocMVP = glGetUniformLocation(p, "mvp");
	uniform.M = glGetUniformLocation(p, "M");
	uniform.CameraPosition = glGetUniformLocation(p, "CameraPosition");
	uniform.LightPosition = glGetUniformLocation(p, "LightPosition");
	uniform.LightDirection = glGetUniformLocation(p, "LightDirection");
	uniform.Ka = glGetUniformLocation(p, "Ka");
	uniform.Kd = glGetUniformLocation(p, "Kd");
	uniform.Ks = glGetUniformLocation(p, "Ks");
	uniform.Ambient = glGetUniformLocation(p, "Ambient");
	uniform.Diffuse = glGetUniformLocation(p, "Diffuse");
	uniform.Specular = glGetUniformLocation(p, "Specular");
	uniform.Shininess = glGetUniformLocation(p, "Shininess");
	uniform.Constant = glGetUniformLocation(p, "Constant");
	uniform.Linear = glGetUniformLocation(p, "Linear");
	uniform.Quadratic = glGetUniformLocation(p, "Quadratic");
	uniform.Exponent = glGetUniformLocation(p, "Exponent");
	uniform.Cutoff = glGetUniformLocation(p, "Cutoff");
	uniform.LightMode = glGetUniformLocation(p, "LightMode");
	uniform.PerPixelLightingMode = glGetUniformLocation(p, "PerPixelLightingMode");

	if (success)
		glUseProgram(p);
    else
    {
        system("pause");
        exit(123);
    }
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, vector<GLfloat>& normals, tinyobj::shape_t* shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//maxs = max(maxs, attrib->vertices.at(i));
		if (i % 3 == 0)
		{

			xVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minX)
			{
				minX = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxX)
			{
				maxX = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 1)
		{
			yVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minY)
			{
				minY = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxY)
			{
				maxY = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 2)
		{
			zVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minZ)
			{
				minZ = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxZ)
			{
				maxZ = attrib->vertices.at(i);
			}
		}
	}

	float offsetX = (maxX + minX) / 2;
	float offsetY = (maxY + minY) / 2;
	float offsetZ = (maxZ + minZ) / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		if (offsetX != 0 && i % 3 == 0)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetX;
		}
		else if (offsetY != 0 && i % 3 == 1)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetY;
		}
		else if (offsetZ != 0 && i % 3 == 2)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetZ;
		}
	}

	float greatestAxis = maxX - minX;
	float distanceOfYAxis = maxY - minY;
	float distanceOfZAxis = maxZ - minZ;

	if (distanceOfYAxis > greatestAxis)
	{
		greatestAxis = distanceOfYAxis;
	}

	if (distanceOfZAxis > greatestAxis)
	{
		greatestAxis = distanceOfZAxis;
	}

	float scale = greatestAxis / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i) / scale;
	}
	size_t index_offset = 0;
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++) {
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++) {
			// access to vertex
			tinyobj::index_t idx = shape->mesh.indices[index_offset + v];
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 0]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 1]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 2]);
			// Optional: vertex colors
			colors.push_back(attrib->colors[3 * idx.vertex_index + 0]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 1]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 2]);
			// Optional: vertex normals
			if (idx.normal_index >= 0) {
				normals.push_back(attrib->normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 2]);
			}
		}
		index_offset += fv;
	}
}

string GetBaseDir(const string& filepath) {
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;
	vector<GLfloat> normals;

	string err;
	string warn;

	string base_dir = GetBaseDir(model_path); // handle .mtl with relative path

#ifdef _WIN32
	base_dir += "\\";
#else
	base_dir += "/";
#endif

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str(), base_dir.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Material size %d\n", int(shapes.size()), int(materials.size()));
	model tmp_model;

	vector<PhongMaterial> allMaterial;
	for (int i = 0; i < materials.size(); i++)
	{
		PhongMaterial material;
		material.Ka = Vector3(materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
		material.Kd = Vector3(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
		material.Ks = Vector3(materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
		allMaterial.push_back(material);
	}

	for (int i = 0; i < shapes.size(); i++)
	{

		vertices.clear();
		colors.clear();
		normals.clear();
		normalization(&attrib, vertices, colors, normals, &shapes[i]);
		// printf("Vertices size: %d", vertices.size() / 3);

		Shape tmp_shape;
		glGenVertexArrays(1, &tmp_shape.vao);
		glBindVertexArray(tmp_shape.vao);

		glGenBuffers(1, &tmp_shape.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		tmp_shape.vertex_count = vertices.size() / 3;

		glGenBuffers(1, &tmp_shape.p_color);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_color);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &tmp_shape.p_normal);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_normal);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GL_FLOAT), &normals.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		// not support per face material, use material of first face
		if (allMaterial.size() > 0)
			tmp_shape.material = allMaterial[shapes[i].mesh.material_ids[0]];
		tmp_model.shapes.push_back(tmp_shape);
	}
	shapes.clear();
	materials.clear();
	models.push_back(tmp_model);
}

void initParameter()
{
	// [TODO] Setup some parameters if you need
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 0.001;
	proj.farClip = 100.0;
	proj.fovy = 80;
	proj.aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	lightInformation[0].Position = Vector3(1.0f, 1.0f, 1.0f);
	lightInformation[0].Direction = Vector3(1.0f, 1.0f, 1.0f);
	lightInformation[0].Diffuse = Vector3(1.0f, 1.0f, 1.0f);
	lightInformation[0].Ambient = Vector3(0.15f, 0.15f, 0.15f);
	lightInformation[0].Specular = Vector3(1.0f, 1.0f, 1.0f);
	lightInformation[0].Cutoff = 0.0f;
	lightInformation[0].Constant = 0.0f;
	lightInformation[0].Linear = 0.0f;
	lightInformation[0].Quadratic = 0.0f;
	lightInformation[0].Exponent = 0.0f;

	lightInformation[1].Position = Vector3(0.0f, 2.0f, 1.0f);
	lightInformation[1].Direction = Vector3(0.0f, 0.0f, 0.0f);
	lightInformation[1].Diffuse = Vector3(1.0f, 1.0f, 1.0f);
	lightInformation[1].Ambient = Vector3(0.15f, 0.15f, 0.15f);
	lightInformation[1].Specular = Vector3(1.0f, 1.0f, 1.0f);
	lightInformation[1].Cutoff = 0.0f;
	lightInformation[1].Constant = 0.01f;
	lightInformation[1].Linear = 0.8f;
	lightInformation[1].Quadratic = 0.1f;
	lightInformation[1].Exponent = 0.0f;

	lightInformation[2].Position = Vector3(0.0f, 0.0f, 2.0f);
	lightInformation[2].Direction = Vector3(0.0f, 0.0f, -1.0f);;
	lightInformation[2].Diffuse = Vector3(1.0f, 1.0f, 1.0f);
	lightInformation[2].Ambient = Vector3(0.15f, 0.15f, 0.15f);
	lightInformation[2].Specular = Vector3(1.0f, 1.0f, 1.0f);
	lightInformation[2].Cutoff = 30.0f;
	lightInformation[2].Constant = 0.05f;
	lightInformation[2].Linear = 0.3f;
	lightInformation[2].Quadratic = 0.6f;
	lightInformation[2].Exponent = 50.0f;

	Shininess = 64.0f;

	setViewingMatrix();
	setPerspective();	//set default projection matrix as perspective matrix
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);
	vector<string> model_list{ "../NormalModels/bunny5KN.obj", "../NormalModels/dragon10KN.obj", "../NormalModels/lucy25KN.obj", "../NormalModels/teapot4KN.obj", "../NormalModels/dolphinN.obj"};
	// [TODO] Load five model at here
	for (string model_path : model_list) {
		LoadModels(model_path);
	}
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char*)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char*)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char*)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char*)glGetStringi(GL_EXTENSIONS, i) << endl;
		}
	}
}


int main(int argc, char **argv)
{
    // initial glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fix compilation on OS X
#endif

    
    // create window
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "110062222 HW2", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    
    // load OpenGL function pointer
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
	// register glfw callback functions
    glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

    glfwSetFramebufferSizeCallback(window, ChangeSize);
	glEnable(GL_DEPTH_TEST);
	// Setup render context
	setupRC();

	// main loop
    while (!glfwWindowShouldClose(window))
    {
        // render
        RenderScene();
        
        // swap buffer from back to front
        glfwSwapBuffers(window);
        
        // Poll input event
        glfwPollEvents();
    }
	
	// just for compatibiliy purposes
	return 0;
}
