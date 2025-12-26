/* 
Controls summary:
- 1: Base translate (mouse X/Z)
- 2: Base spin (mouse X)
- 3: Shoulder + Elbow (mouse Y / X)
- 4: Wrist bend + Wrist twist (mouse Y / X)
- 5: Fingers (mouse Y / X)
- SPACE: Toggle teapot follow (only if CanGrabTeapot() is true when not already following).
- ESC: Quit
*/

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <cmath> // std::abs

//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// ROBOT ARM CONTROLS
float BaseTransX = -0.5f;  // 0
float BaseTransZ = 0;
float BaseSpin   = 0;      // 1
float ShoulderAng   = -10; // 2
float ElbowAng      = -120;
float WristAng      = 90;  // 3
float WristTwistAng = 10;
float FingerAng1    = 45;  // 4
float FingerAng2    = -90;

// === Extra credit state ===
bool TeapotFollowWrist = false; // SPACE 토글 상태

// ---- 잡기 판정 파라미터 (필요시 살짝 조정 가능) ----
const float kGrabDist  = 0.35f; // palm-주전자 중심 거리 허용치 (모델 스케일 고려 여유있게)
const float kMinClose1 = 10.0f; // FingerAng1 >= 10° 이면 '닫힘'
const float kMinClose2 = 20.0f; // |FingerAng2| >= 20° 이면 '닫힘'

// Forward declarations for helpers (used in processInput/myDisplay)
glm::mat4 ComputePalmMatrix();
inline glm::vec3 GetWorldPos(const glm::mat4& M);
bool CanGrabTeapot();
inline glm::mat4 TeapotLocalXform();

// ROBOT COLORS
GLfloat Ground[] = { 0.5f, 0.5f, 0.5f };
GLfloat Arms[] = { 0.5f, 0.5f, 0.5f };
GLfloat Joints[] = { 0.0f, 0.27f, 0.47f };
GLfloat Fingers[] = { 0.59f, 0.0f, 0.09f };
GLfloat FingerJoints[] = { 0.5f, 0.5f, 0.5f };

// USER INTERFACE GLOBALS
int LeftButtonDown = 0;    // MOUSE STUFF
int RobotControl = 0;

// settings
const unsigned int SCR_WIDTH = 768;
const unsigned int SCR_HEIGHT = 768;

// camera
//Camera camera(glm::vec3(0.0f, 0.8f, 1.2f), glm::vec3(0.0f, 0.5f, 0.0f), -90.f, 0.0f);
Camera camera(glm::vec3(0.0f, 1.5f, 2.5f), glm::vec3(0.0f, 1.0f, 0.0f), -90.f, -15.0f);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

// light information
glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

// shader
Shader* PhongShader;
Shader* FloorShader;

// ObjectModel
Model* ourObjectModel;
const char* ourObjectPath = "src/models/teapot.obj";
// 바닥 위 기본 위치/스케일 (주전자 바닥 상태에서 사용할 기본 변환)
glm::mat4 objectXform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f)), glm::vec3(0.08f, 0.08f, 0.08f));

bool gPrevFollowActive = false;     // 이전 프레임 follow 상태(전이 감지)
glm::mat4 gGrip2Teapot = glm::mat4(1.0f); // palm->teapot 상대자세

static bool prevFollow = false;  // 이전 프레임의 팔로우 상태


// HOUSE KEEPING
void initGL(GLFWwindow** window);
void setupShader();
void destroyShader();
void createGLPrimitives();
void destroyGLPrimitives();

// CALLBACKS
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void processInput(GLFWwindow* window, int key, int scancode, int action, int mods);

// DRAWING ROBOT PARTS
void DrawGroundPlane(glm::mat4 model);
void DrawJoint(glm::mat4 model);
void DrawBase(glm::mat4 model);
void DrawArmSegment(glm::mat4 model);
void DrawWrist(glm::mat4 model);
void DrawFingerBase(glm::mat4 model);
void DrawFingerTip(glm::mat4 model);

void DrawObject(glm::mat4 model);
bool hasTextures = false; 

void myDisplay()
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Ground
	glm::mat4 model = glm::mat4(1.0f);
	DrawGroundPlane(model);

	// === ROBOT DRAW CALLS ===
	glm::mat4 R = glm::mat4(1.0f);

	// 로봇 베이스 위치/회전(마우스로 제어하는 값 반영)
	R = glm::translate(R, glm::vec3(BaseTransX, 0.0f, BaseTransZ));
	R = glm::rotate(R, glm::radians(BaseSpin), glm::vec3(0.0f, 1.0f, 0.0f));

	// 베이스
	DrawBase(R);

	// 어깨
	glm::mat4 shoulder = R;
	shoulder = glm::translate(shoulder, glm::vec3(0.0f, 0.40f, 0.0f));
	shoulder = glm::rotate(shoulder, glm::radians(ShoulderAng), glm::vec3(0.0f, 0.0f, 1.0f));
	DrawArmSegment(shoulder);

	// 팔꿈치
	glm::mat4 elbow = shoulder;
	elbow = glm::translate(elbow, glm::vec3(0.0f, 0.50f, 0.0f));
	elbow = glm::rotate(elbow, glm::radians(ElbowAng), glm::vec3(0.0f, 0.0f, 1.0f));
	DrawArmSegment(elbow);

	// 손목
	glm::mat4 wrist = elbow;
	wrist = glm::translate(wrist, glm::vec3(0.0f, 0.50f, 0.0f));
	wrist = glm::rotate(wrist, glm::radians(WristAng), glm::vec3(0.0f, 0.0f, 1.0f));
	wrist = glm::rotate(wrist, glm::radians(WristTwistAng), glm::vec3(0.0f, 1.0f, 0.0f));
	DrawWrist(wrist);

	// 손바닥(palm): wrist 기준 Y +0.2
	glm::mat4 palm = wrist;
	palm = glm::translate(palm, glm::vec3(0.0f, 0.10f, 0.0f));

	// 손가락 2개 (palm 기준 좌우로만 오프셋)
	glm::mat4 f1 = palm;
	f1 = glm::translate(f1, glm::vec3(+0.06f, 0.0f, 0.0f));
	f1 = glm::rotate(f1, glm::radians(FingerAng1), glm::vec3(0.0f, 0.0f, 1.0f));
	DrawFingerBase(f1);

	glm::mat4 f1tip = f1;
	f1tip = glm::translate(f1tip, glm::vec3(0.0f, 0.35f, 0.0f));
	f1tip = glm::rotate(f1tip, glm::radians(FingerAng2), glm::vec3(0.0f, 0.0f, 1.0f));
	DrawFingerTip(f1tip);

	glm::mat4 f2 = palm;
	f2 = glm::translate(f2, glm::vec3(-0.06f, 0.0f, 0.0f));
	f2 = glm::rotate(f2, glm::radians(-FingerAng1), glm::vec3(0.0f, 0.0f, 1.0f));
	DrawFingerBase(f2);

	glm::mat4 f2tip = f2;
	f2tip = glm::translate(f2tip, glm::vec3(0.0f, 0.35f, 0.0f));
	f2tip = glm::rotate(f2tip, glm::radians(-FingerAng2), glm::vec3(0.0f, 0.0f, 1.0f));
	DrawFingerTip(f2tip);

	// === Teapot draw (Extra credit) ===
	// SPACE 토글: 단, processInput에서 CanGrabTeapot() 만족할 때만 TeapotFollowWrist 가 true가 됨.
	if (TeapotFollowWrist)
	{
        glm::mat4 teapotWorld = palm * TeapotLocalXform();
        objectXform = teapotWorld;
		DrawObject(teapotWorld);
	}
	else
	{
         if (prevFollow) {
            glm::vec4 t = objectXform[3];  // GLM은 열 우선, 3번째 열이 translation
            t.y = 0.0f;                    // 바닥 y=0에 놓기 (필요하면 약간의 +바이어스)
            objectXform[3] = t;            // 회전/스케일은 유지, 위치만 드롭
        }
		DrawObject(objectXform); // 바닥 위 기본 주전자 지금 위치기준으로 떨기기 
	}
    prevFollow = TeapotFollowWrist;
}

// ======================================================================
// Main / Setup
// ======================================================================

int main()
{
	GLFWwindow* window = NULL;

	initGL(&window);
	setupShader();
	createGLPrimitives();

	glEnable(GL_DEPTH_TEST);

	// render loop
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// view/projection transformations
		PhongShader->use();
		PhongShader->setMat4("projection", glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f));
		PhongShader->setMat4("view", camera.GetViewMatrix());
		PhongShader->setVec3("viewPos", camera.Position);
		PhongShader->setVec3("lightPos", camera.Position);

		FloorShader->use();
		FloorShader->setMat4("projection", glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f));
		FloorShader->setMat4("view", camera.GetViewMatrix());
		FloorShader->setVec3("viewPos", camera.Position);
		FloorShader->setVec3("lightPos", camera.Position);

		// render
		myDisplay();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	destroyGLPrimitives();
	destroyShader();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

void initGL(GLFWwindow** window)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "The Robot Arm", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(*window);
	glfwSetFramebufferSizeCallback(*window, framebuffer_size_callback);
	glfwSetCursorPosCallback(*window, mouse_callback);
	glfwSetMouseButtonCallback(*window, mouse_button_callback);
	glfwSetKeyCallback(*window, processInput);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		exit(-1);
	}
}

void setupShader()
{
    PhongShader = new Shader(
        "src/shaders/model_loading.vert",
        "src/shaders/model_loading.frag"
    );
    PhongShader->use();
    PhongShader->setVec3("lightColor", lightColor);

    FloorShader = new Shader(
        "src/shaders/phong.vert",
        "src/shaders/phong.frag"
    );
    FloorShader->use();
    FloorShader->setVec3("lightColor", lightColor);
}

void destroyShader()
{
	delete PhongShader;
	delete FloorShader;
}

// ======================================================================
// Input Callbacks
// ======================================================================

void processInput(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5 && action == GLFW_PRESS)
	{
		RobotControl = key - GLFW_KEY_1;
	}
	else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
	{
		// Extra credit: '잡기'는 손가락이 닫혀 있고(pose), palm-주전자 거리가 충분히 가까울 때만 ON
		if (!TeapotFollowWrist)
		{
			if (CanGrabTeapot())
			{
				TeapotFollowWrist = true; // 손에 듦
				// (옵션) 시각적 강화: 더 쥔 포즈로 각도 보정 가능
				// FingerAng1 = 45.0f; FingerAng2 = -90.0f;
			}
		}
		else
		{
			// 손에 들고 있을 땐 언제든 놓기
			TeapotFollowWrist = false;
			// (옵션) 놓을 때 펴는 포즈
			// FingerAng1 = 20.0f; FingerAng2 = -40.0f;
		}
	}
	else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, true);
	}
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = (float) xpos;
		lastY = (float) ypos;
		firstMouse = false;
	}

	float xoffset = (float) (xpos - lastX) / SCR_WIDTH;
	float yoffset = (float) (lastY - ypos) / SCR_HEIGHT; // reversed since y-coordinates go from bottom to top

	lastX = (float) xpos;
	lastY = (float) ypos;

	if (LeftButtonDown)
	{
		switch (RobotControl)
		{
		case 0: BaseTransX += xoffset; BaseTransZ -= yoffset; break;
		case 1: BaseSpin += xoffset * 180 ; break;
		case 2: ShoulderAng += yoffset * -90; ElbowAng += xoffset * 90; break;
		case 3: WristAng += yoffset * -180; WristTwistAng += xoffset * 180; break;
		case 4: FingerAng1 += yoffset * 90; FingerAng2 += xoffset * 180; break;
		}
	} 
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		LeftButtonDown = 1;
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		LeftButtonDown = 0;
	}
}

// ======================================================================
// Geometry / Primitives
// ======================================================================

class Primitive {
public:
	Primitive() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);
	}
	~Primitive() {
		if (!ebo) glDeleteBuffers(1, &ebo);
		if (!vbo) glDeleteBuffers(1, &vbo);
		if (!VAO) glDeleteVertexArrays(1, &VAO);
	}
	void Draw() {
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLE_STRIP, IndexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

protected:
	unsigned int VAO = 0, vbo = 0, ebo = 0;
	unsigned int IndexCount = 0;
	float height = 1.0f;
    float radius[2] = { 1.0f, 1.0f };
};

class Cylinder : public Primitive {
public:
	Cylinder(float bottomRadius = 0.5f, float topRadius = 0.5f, int NumSegs = 16);
};

class Sphere : public Primitive {
public:
	Sphere(int NumSegs = 16);
};

class Plane : public Primitive {
public:
	Plane();
	void Draw() {
		glBindVertexArray(VAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		glDrawElements(GL_TRIANGLE_STRIP, IndexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
private:
	unsigned int floorTexture;
};

Sphere* unitSphere;
Plane* groundPlane;
Cylinder* unitCylinder;
Cylinder* unitCone;

void createGLPrimitives()
{
	unitSphere = new Sphere();
	groundPlane = new Plane();
	unitCylinder = new Cylinder();
	unitCone = new Cylinder(0.5f, 0.0f);

	// Load Object Model
	ourObjectModel = new Model(ourObjectPath);
	hasTextures = (ourObjectModel->textures_loaded.size() == 0) ? 0 : 1;
}
void destroyGLPrimitives()
{
	delete unitSphere;
	delete groundPlane;
	delete unitCylinder;
	delete unitCone;

	delete ourObjectModel;
}

void DrawGroundPlane(glm::mat4 model)
{
	FloorShader->use();
	FloorShader->setMat4("model", model);
	groundPlane->Draw();
}

void DrawJoint(glm::mat4 model)
{
	glm::mat4 Mat1 = glm::scale(glm::mat4(1.0f), glm::vec3(0.15f, 0.15f, 0.12f));
	Mat1 = glm::rotate(Mat1, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	PhongShader->use();
	Mat1 = model * Mat1;
	PhongShader->setMat4("model", Mat1);
	PhongShader->setVec3("ObjColor", glm::vec3(Joints[0], Joints[1], Joints[2]));
	PhongShader->setInt("hasTextures", false);
	unitCylinder->Draw();
}
void DrawBase(glm::mat4 model)
{
	glm::mat4 Base = glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, 0.025f, 0.2f));
	glm::mat4 InBase = glm::inverse(Base);

	PhongShader->use();
	Base = model * Base;
	PhongShader->setMat4("model", Base);
	PhongShader->setVec3("ObjColor", glm::vec3(Joints[0], Joints[1], Joints[2]));
	PhongShader->setInt("hasTextures", false);
	unitCylinder->Draw();

	glm::mat4 Mat1 = glm::translate(InBase, glm::vec3(0.0f, 0.2f, 0.0f));
	Mat1 = glm::scale(Mat1, glm::vec3(0.1f, 0.4f, 0.1f));

	Mat1 = Base * Mat1;
	PhongShader->setMat4("model", Mat1);
	PhongShader->setVec3("ObjColor", glm::vec3(Arms[0], Arms[1], Arms[2]));
	unitCylinder->Draw();

	glm::mat4 Mat2 = glm::translate(InBase, glm::vec3(0.0f, 0.4f, 0.0f));
	Mat2 = Base * Mat2;
	PhongShader->setMat4("model", Mat2);
	DrawJoint(Mat2);
}
void DrawArmSegment(glm::mat4 model)
{
	glm::mat4 Base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.2f, 0.0f));
	Base = glm::scale(Base, glm::vec3(0.1f, 0.5f, 0.1f));
	glm::mat4 InBase = glm::inverse(Base);

	PhongShader->use();
	Base = model * Base;
	PhongShader->setMat4("model", Base);
	PhongShader->setVec3("ObjColor", glm::vec3(Arms[0], Arms[1], Arms[2]));
	PhongShader->setInt("hasTextures", false);
	unitCylinder->Draw();

	glm::mat4 Mat1 = glm::translate(InBase, glm::vec3(0.0f, 0.5f, 0.0f));;
	Mat1 = Base * Mat1;
	PhongShader->setMat4("model", Mat1);
	DrawJoint(Mat1);
}
void DrawWrist(glm::mat4 model)
{
	glm::mat4 Base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.1f, 0.0f));
	Base = glm::scale(Base, glm::vec3(0.08f, 0.2f, 0.08f));
	glm::mat4 InBase = glm::inverse(Base);

	PhongShader->use();
	Base = model * Base;
	PhongShader->setMat4("model", Base);
	PhongShader->setVec3("ObjColor", glm::vec3(Fingers[0], Fingers[1], Fingers[2]));
	PhongShader->setInt("hasTextures", false);
	unitCylinder->Draw();

	glm::mat4 Mat1 = glm::translate(InBase, glm::vec3(0.0f, 0.2f, 0.0f));
	Mat1 = glm::scale(Mat1, glm::vec3(0.06f, 0.06f, 0.06f));

	Mat1 = Base * Mat1;
	PhongShader->setMat4("model", Mat1);
	PhongShader->setVec3("ObjColor", glm::vec3(FingerJoints[0], FingerJoints[1], FingerJoints[2]));
	unitSphere->Draw(); 
}
void DrawFingerBase(glm::mat4 model)
{
	glm::mat4 Base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.2f, 0.0f));
	Base = glm::scale(Base, glm::vec3(0.05f, 0.3f, 0.05f));
	glm::mat4 InBase = glm::inverse(Base);

	PhongShader->use();
	Base = model * Base;
	PhongShader->setMat4("model", Base);
	PhongShader->setVec3("ObjColor", glm::vec3(Fingers[0], Fingers[1], Fingers[2]));
	PhongShader->setInt("hasTextures", false);
	unitCylinder->Draw();

	glm::mat4 Mat1 = glm::translate(InBase, glm::vec3(0.0f, 0.35f, 0.0f));
	Mat1 = glm::scale(Mat1, glm::vec3(0.05f, 0.05f, 0.05f));

	Mat1 = Base * Mat1;
	PhongShader->setMat4("model", Mat1);
	PhongShader->setVec3("ObjColor", glm::vec3(FingerJoints[0], FingerJoints[1], FingerJoints[2]));
	unitSphere->Draw();
}
void DrawFingerTip(glm::mat4 model)
{
	glm::mat4 Base = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f, 0.25f, 0.05f));
	Base = glm::translate(Base, glm::vec3(0.0f, 0.4f, 0.0f));

	PhongShader->use();
	Base = model * Base;
	PhongShader->setMat4("model", Base);
	PhongShader->setVec3("ObjColor", glm::vec3(Fingers[0], Fingers[1], Fingers[2]));
	PhongShader->setInt("hasTextures", false);
	unitCone->Draw();
}

void DrawObject(glm::mat4 model)
{
	PhongShader->use();
	PhongShader->setMat4("model", model);
	PhongShader->setVec3("ObjColor", glm::vec3(1.0f, 1.0f, 0.0f));
	PhongShader->setInt("hasTextures", hasTextures);
	ourObjectModel->Draw(*PhongShader);
}

// ======================================================================
// Helpers for grabbing logic (Extra credit)
// ======================================================================

glm::mat4 ComputePalmMatrix()
{
	// myDisplay의 계층과 동일하게 구성해서 palm의 월드 변환을 계산
	glm::mat4 R = glm::mat4(1.0f);
	R = glm::translate(R, glm::vec3(BaseTransX, 0.0f, BaseTransZ));
	R = glm::rotate(R, glm::radians(BaseSpin), glm::vec3(0.0f, 1.0f, 0.0f));

	glm::mat4 shoulder = R;
	shoulder = glm::translate(shoulder, glm::vec3(0.0f, 0.40f, 0.0f));
	shoulder = glm::rotate(shoulder, glm::radians(ShoulderAng), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::mat4 elbow = shoulder;
	elbow = glm::translate(elbow, glm::vec3(0.0f, 0.50f, 0.0f));
	elbow = glm::rotate(elbow, glm::radians(ElbowAng), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::mat4 wrist = elbow;
	wrist = glm::translate(wrist, glm::vec3(0.0f, 0.50f, 0.0f));
	wrist = glm::rotate(wrist, glm::radians(WristAng), glm::vec3(0.0f, 0.0f, 1.0f));
	wrist = glm::rotate(wrist, glm::radians(WristTwistAng), glm::vec3(0.0f, 1.0f, 0.0f));

	glm::mat4 palm = wrist;
	palm = glm::translate(palm, glm::vec3(0.0f, 0.2f, 0.0f));
	return palm;
}

inline glm::vec3 GetWorldPos(const glm::mat4& M)
{
	// GLM column-major: 마지막 열이 translation
	return glm::vec3(M[3]);
}

inline glm::mat4 TeapotLocalXform()
{
	// 손바닥 위에서 살짝 오른쪽/위로 올려서 '집은' 느낌 + 크기조절
	glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(0.04f, 0.08f, 0.0f));
	glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.08f));
	return T * R * S;
}

bool CanGrabTeapot()
{
	if (TeapotFollowWrist) return false; // 이미 들고 있으면 새로 잡기 X

	// 1) 손가락이 충분히 '쥔' 상태인지 (완화 조건)
	bool fingersClosed =
		(FingerAng1 >= kMinClose1) && (std::abs(FingerAng2) >= kMinClose2);
	if (!fingersClosed) return false;

	// 2) 손바닥과 주전자 중심이 충분히 가까운지
	glm::vec3 palmPos   = GetWorldPos(ComputePalmMatrix());
	glm::vec3 teapotPos = GetWorldPos(objectXform); // teapot 모델 원점이 완전 중심이 아닐 수 있어 거리를 여유있게
	float dist = glm::length(palmPos - teapotPos);
	return dist <= kGrabDist;
}

// ======================================================================
// Sphere / Plane / Cylinder implementations
// ======================================================================

Sphere::Sphere(int NumSegs)
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	const unsigned int X_SEGMENTS = NumSegs;
	const unsigned int Y_SEGMENTS = NumSegs;
	const float PI = (float)3.14159265359;

	for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
	{
		for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
		{
			float xSegment = (float)x / (float)X_SEGMENTS;
			float ySegment = (float)y / (float)Y_SEGMENTS;
			float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
			float yPos = std::cos(ySegment * PI);
			float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

			positions.push_back(glm::vec3(xPos, yPos, zPos));
			normals.push_back(glm::vec3(xPos, yPos, zPos));
		}
	}

	bool oddRow = false;
	for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
	{
		if (!oddRow)
		{
			for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
			{
				indices.push_back(y * (X_SEGMENTS + 1) + x);
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
			}
		}
		else
		{
			for (int x = X_SEGMENTS; x >= 0; --x)
			{
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
				indices.push_back(y * (X_SEGMENTS + 1) + x);
			}
		}
		oddRow = !oddRow;
	}

	IndexCount = (unsigned int)indices.size();

	std::vector<float> data;
	for (size_t i = 0; i < positions.size(); ++i)
	{
		data.push_back(positions[i].x);
		data.push_back(positions[i].y);
		data.push_back(positions[i].z);
		if (normals.size() > 0)
		{
			data.push_back(normals[i].x);
			data.push_back(normals[i].y);
			data.push_back(normals[i].z);
		}
	}
	
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
	GLsizei stride = (3 + 3) * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
}

unsigned int loadTexture(char const* path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum format = GL_RGB;
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, (GLint)format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}

	return textureID;
}

Plane::Plane()
{
	float data[] = {
		// positions           // normals          // texcoords
		-10.0f, 0.0f, -10.0f,  0.0f, 1.0f,  0.0f,  0.0f,  0.0f,
		 10.0f, 0.0f, -10.0f,  0.0f, 1.0f,  0.0f,  10.0f, 0.0f,
		 10.0f, 0.0f,  10.0f,  0.0f, 1.0f,  0.0f,  10.0f, 10.0f,
		-10.0f, 0.0f,  10.0f,  0.0f, 1.0f,  0.0f,  0.0f,  10.0f
	};
	unsigned int indices[] = { 0, 1, 3, 2 };

	IndexCount = sizeof(indices)/sizeof(unsigned int);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	GLsizei stride = (3 + 3 + 2) * sizeof(float);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

	glBindVertexArray(0);

	unsigned int floorTexture = loadTexture("src/textures/wood.png");
	FloorShader->use();
	FloorShader->setInt("texture1", 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, floorTexture);
}

Cylinder::Cylinder(float bottomRadius, float topRadius, int NumSegs)
{
	radius[0] = bottomRadius; radius[1] = topRadius;

	std::vector<glm::vec3> base;
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	const float PI = (float)3.14159265359;
	float sectorStep = 2 * PI / NumSegs;
	float sectorAngle;

	for (int i = 0; i <= NumSegs; ++i)
	{
		sectorAngle = i * sectorStep;
		float xPos = std::sin(sectorAngle);
		float yPos = 0;
		float zPos = std::cos(sectorAngle);

		base.push_back(glm::vec3(xPos, yPos, zPos));
	}

	// side of cylinder
	for (int i = 0; i < 2; ++i)
	{
		float h = -height / 2.0f + i * height; // -h/2 to h/2   
		for (int j = 0; j <= NumSegs; ++j)
		{
			positions.push_back(glm::vec3(base[j].x * radius[i], h, base[j].z * radius[i]));
			normals.push_back(glm::vec3(base[j].x, h, base[j].z));
		}
	}

	// base/top centers
	int baseCenterIndex = (int)positions.size();
	int topCenterIndex = baseCenterIndex + NumSegs + 1;

	// base & top circles
	for (int i = 0; i < 2; ++i)
	{
		float h = -height / 2.0f + i * height;
		float ny = (float)-1 + i * 2;

		positions.push_back(glm::vec3(0, h, 0));
		normals.push_back(glm::vec3(0, ny, 0));

		for (int j = 0; j < NumSegs; ++j)
		{
			positions.push_back(glm::vec3(base[j].x * radius[i], h, base[j].z * radius[i]));
			normals.push_back(glm::vec3(0, ny, 0));
		}
	}

	// indices - side
	int k1 = 0;
	int k2 = NumSegs + 1;
	for (int i = 0; i < NumSegs; ++i, ++k1, ++k2)
	{
		indices.push_back(k1);
		indices.push_back(k1 + 1);
		indices.push_back(k2);

		indices.push_back(k2);
		indices.push_back(k1 + 1);
		indices.push_back(k2 + 1);
	}

	// indices - base
	for (int i = 0, k = baseCenterIndex + 1; i < NumSegs; ++i, ++k)
	{
		if (i < NumSegs - 1)
		{
			indices.push_back(baseCenterIndex);
			indices.push_back(k + 1);
			indices.push_back(k);
		}
		else
		{
			indices.push_back(baseCenterIndex);
			indices.push_back(baseCenterIndex + 1);
			indices.push_back(k);
		}
	}

	// indices - top
	for (int i = 0, k = topCenterIndex + 1; i < NumSegs; ++i, ++k)
	{
		if (i < NumSegs - 1)
		{
			indices.push_back(topCenterIndex);
			indices.push_back(k);
			indices.push_back(k + 1);
		}
		else
		{
			indices.push_back(topCenterIndex);
			indices.push_back(k);
			indices.push_back(topCenterIndex + 1);
		}
	}

	IndexCount = (unsigned int)indices.size();

	std::vector<float> data;
	for (size_t i = 0; i < positions.size(); ++i)
	{
		data.push_back(positions[i].x);
		data.push_back(positions[i].y);
		data.push_back(positions[i].z);

		if (normals.size() > 0)
		{
			data.push_back(normals[i].x);
			data.push_back(normals[i].y);
			data.push_back(normals[i].z);
		}
	}
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
	GLsizei stride = (3 + 3) * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
}
