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
const int WINDOW_HEIGHT = 600;

bool mouse_pressed = false;
bool wireframe_mode_on = false;
int starting_press_x = -1;
int starting_press_y = -1;

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	ViewCenter = 3,
	ViewEye = 4,
	ViewUp = 5,
};

GLint iLocMVP;

vector<string> filenames; // .obj filename list

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form
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

enum ProjMode
{
	Orthogonal = 0,
	Perspective = 1,
};
ProjMode cur_proj_mode = Orthogonal;
TransMode cur_trans_mode = GeoTranslation;

Matrix4 view_matrix;
Matrix4 project_matrix;


typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	int materialId;
	int indexCount;
	GLuint m_texture;
} Shape;
Shape quad;
Shape m_shpae;
vector<Shape> m_shape_list;
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
// FINISHED CG07 P.39
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
// FINISHED CG07 P.40
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
// FINISHED CG07 p.41
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
// FINISHED CG07 p.42
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
// FINISHED CG07 p.43
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
// FINISHED CG07 p.68
void setViewingMatrix()
{ 
	Vector3 P1_P2 = main_camera.center - main_camera.position;
	Vector3 P1_P3 = main_camera.up_vector;

	GLfloat P1P2[3] = { P1_P2.x, P1_P2.y, P1_P2.z };
	GLfloat P1P3[3] = { P1_P3.x, P1_P3.y, P1_P3.z };

	GLfloat Rz[3] = {-P1_P2.x, -P1_P2.y, -P1_P2.z };
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

// [TODO] compute orthogonal projection matrix
// FINISHED CG08 P.52
void setOrthogonal()
{
	cur_proj_mode = Orthogonal;

	GLfloat Right_Left = proj.right - proj.left;
	GLfloat Top_Bottom = proj.top - proj.bottom;
	GLfloat Far_Near = proj.farClip - proj.nearClip;
	GLfloat tx = -(proj.right + proj.left) / Right_Left;
	GLfloat ty = -(proj.top + proj.bottom) / Top_Bottom;
	GLfloat tz = -(proj.farClip + proj.nearClip) / Far_Near;

	if (proj.aspect >= 1) {
		project_matrix = Matrix4(
			2 / Right_Left / proj.aspect, 0, 0, tx,
			0, 2 / Top_Bottom, 0, ty,
			0, 0, -2 / Far_Near, tz,
			0, 0, 0, 1
		);
	}
	else {
		project_matrix = Matrix4(
			2 / Right_Left, 0, 0, tx,
			0, 2 / Top_Bottom * proj.aspect, 0, ty,
			0, 0, -2 / Far_Near, tz,
			0, 0, 0, 1
		);
	}
}

// [TODO] compute persepective projection matrix
// FINISHED CG08 P.57
void setPerspective()
{
	cur_proj_mode = Perspective;

	GLfloat Near_Add_Far = proj.nearClip + proj.farClip;
	GLfloat Near_Minus_Far = proj.nearClip - proj.farClip;
	GLfloat Near_Multiply_Far = proj.nearClip * proj.farClip;
	GLfloat f = 1 / tan(proj.fovy * 3.1415926535 / 180  / 2);

	if (proj.aspect >= 1) {
		project_matrix = Matrix4(
			f / proj.aspect, 0, 0, 0,
			0, f, 0, 0,
			0, 0, Near_Add_Far / Near_Minus_Far, 2 * Near_Multiply_Far / Near_Minus_Far,
			0, 0, -1, 0
		);
	}
	else {
		project_matrix = Matrix4(
			f, 0, 0, 0,
			0, f * proj.aspect, 0, 0,
			0, 0, Near_Add_Far / Near_Minus_Far, 2 * Near_Multiply_Far / Near_Minus_Far,
			0, 0, -1, 0
		);
	}
}


// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	// [TODO] change your aspect ratio

	proj.aspect = (GLfloat) width / (GLfloat) height;
	switch (cur_proj_mode)
	{
		case ProjMode::Orthogonal:
			setOrthogonal();
		case ProjMode::Perspective:
			setPerspective();
		default:
			break;
	}
}

void LoadPlaneVertexData() {
	GLfloat vertices[18]{ 1.0, -0.9, -1.0,
		1.0, -0.9,  1.0,
		-1.0, -0.9, -1.0,
		1.0, -0.9,  1.0,
		-1.0, -0.9,  1.0,
		-1.0, -0.9, -1.0 };

	GLfloat colors[18]{ 0.0,1.0,0.0,
		0.0,0.5,0.8,
		0.0,1.0,0.0,
		0.0,0.5,0.8,
		0.0,0.5,0.8,
		0.0,1.0,0.0 };

	glGenVertexArrays(1, &quad.vao);
	glBindVertexArray(quad.vao);

	glGenBuffers(1, &quad.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, quad.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	quad.vertex_count = sizeof(vertices) / sizeof(GLfloat) / 3;

	glGenBuffers(1, &quad.p_color);
	glBindBuffer(GL_ARRAY_BUFFER, quad.p_color);
	glBufferData(GL_ARRAY_BUFFER, sizeof(colors), &colors[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
}

void drawPlane()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	Matrix4 MVP;
	GLfloat mvp[16];

	MVP = project_matrix * view_matrix;
	mvp[0] = MVP[0];  mvp[4] = MVP[1];   mvp[8] = MVP[2];    mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];   mvp[9] = MVP[6];    mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];   mvp[10] = MVP[10];   mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13];  mvp[11] = MVP[14];   mvp[15] = MVP[15];

	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glBindVertexArray(quad.vao);
	glDrawArrays(GL_TRIANGLES, 0, quad.vertex_count);
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

	Matrix4 MVP;
	GLfloat mvp[16];

	// [TODO] multiply all the matrix
	MVP = project_matrix * view_matrix * T * R * S;
	// [TODO] row-major ---> column-major

	mvp[0] = MVP[0];  mvp[4] = MVP[1];   mvp[8] = MVP[2];    mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];   mvp[9] = MVP[6];    mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];   mvp[10] = MVP[10];   mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13];  mvp[11] = MVP[14];   mvp[15] = MVP[15];
		
	wireframe_mode_on ? glPolygonMode(GL_FRONT_AND_BACK, GL_LINE) : glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glBindVertexArray(m_shape_list[cur_idx].vao);
	glDrawArrays(GL_TRIANGLES, 0, m_shape_list[cur_idx].vertex_count);
	drawPlane();
}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	if (action != GLFW_PRESS) return;
	switch (key) {
		case GLFW_KEY_W: {
			wireframe_mode_on = !wireframe_mode_on; 
			break; 
		}
		case GLFW_KEY_Z: {
			--cur_idx;
			if (cur_idx < 0) cur_idx = models.size() - 1;
			break;
		}
		case GLFW_KEY_X: {
			++cur_idx;
			if (cur_idx >= models.size()) cur_idx = 0;
			break;
		}
		case GLFW_KEY_O: {
			setOrthogonal();
			break;
		}
		case GLFW_KEY_P: {
			setPerspective();
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
		case GLFW_KEY_E: {
			cur_trans_mode = TransMode::ViewEye;
			break;
		}
		case GLFW_KEY_C: {
			cur_trans_mode = TransMode::ViewCenter;
			break;
		}
		case GLFW_KEY_U: {
			cur_trans_mode = TransMode::ViewUp;
			break;
		}
		case GLFW_KEY_I: {
			cout << "Matrix Value:\n";
			cout << "Viewing Matrix:\n";
			cout << view_matrix << "\n\n";
			cout << "Projection Matrix:\n";
			cout << project_matrix << "\n\n";
			cout << "Translation Matrix:\n";
			cout << translate(models[cur_idx].position) << "\n\n";
			cout << "Rotation Matrix:\n";
			cout << rotate(models[cur_idx].rotation) << "\n\n";
			cout << "Scaling Matrix:\n";
			cout << scaling(models[cur_idx].scale) << "\n\n";
			break;
		}	
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negtive
	// FINISH
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
		case TransMode::ViewEye:
			main_camera.position.z -= yoffset;
			setViewingMatrix();
			break;
		case TransMode::ViewCenter:
			main_camera.center.z += yoffset;
			setViewingMatrix();
			break;
		case TransMode::ViewUp:
			main_camera.up_vector.z += yoffset;
			setViewingMatrix();
			break;
		default:
			break;
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
	// FINISH
	if (button == GLFW_MOUSE_BUTTON_LEFT)
		mouse_pressed = (action == GLFW_PRESS);
}

double xpos_prev, ypos_prev;

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// [TODO] cursor position callback function
	// FINISH
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
				models[cur_idx].scale.x -= xdiff;
				models[cur_idx].scale.y -= ydiff;
				break;
			case TransMode::GeoRotation:
				models[cur_idx].rotation.y -= xdiff;
				models[cur_idx].rotation.x -= ydiff;
				break;
			case TransMode::ViewEye:
				main_camera.position.x -= xdiff;
				main_camera.position.y += ydiff;
				setViewingMatrix();
				break;
			case TransMode::ViewCenter:
				main_camera.center.x -= xdiff;
				main_camera.center.y -= ydiff;
				setViewingMatrix();
				break;
			case TransMode::ViewUp:
				main_camera.up_vector.x -= xdiff;
				main_camera.up_vector.y -= ydiff;
				setViewingMatrix();
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

	iLocMVP = glGetUniformLocation(p, "mvp");

	if (success)
		glUseProgram(p);
    else
    {
        system("pause");
        exit(123);
    }
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, tinyobj::shape_t* shape)
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
		attrib->vertices.at(i) = attrib->vertices.at(i)/ scale;
	}
	size_t index_offset = 0;
	vertices.reserve(shape->mesh.num_face_vertices.size() * 3);
	colors.reserve(shape->mesh.num_face_vertices.size() * 3);
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
		}
		index_offset += fv;
	}
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;

	string err;
	string warn;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Maerial size %d\n", shapes.size(), materials.size());
	
	normalization(&attrib, vertices, colors, &shapes[0]);

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

	m_shape_list.push_back(tmp_shape);
	model tmp_model;
	models.push_back(tmp_model);


	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	shapes.clear();
	materials.clear();
}

void initParameter()
{
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
	vector<string> model_list{ "../ColorModels/bunny5KC.obj", "../ColorModels/dragon10KC.obj", "../ColorModels/lucy25KC.obj", "../ColorModels/teapot4KC.obj", "../ColorModels/dolphinC.obj"};
	// [TODO] Load five model at here
	for (string model_path : model_list) {
		LoadModels(model_path);
	}
	LoadPlaneVertexData();
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
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "110062222 HW1", NULL, NULL);
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
