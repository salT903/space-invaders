#include <cstdio>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define GL_ERROR_CASE(glerror)\
    case glerror: snprintf(error, sizeof(error), "%s", #glerror)

inline void gl_debug(const char *file, int line){
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR){
        char error[128];

        switch(err) {
            GL_ERROR_CASE(GL_INVALID_ENUM); //0x0500 given only for local function problems (should never throw)
              break;
            GL_ERROR_CASE(GL_INVALID_VALUE); //0x0501 given when an illegal value is passed to a function (also should never throw)
              break;
            GL_ERROR_CASE(GL_INVALID_OPERATION); //0x0502 given when the set state of a command is not legal
              break;
            GL_ERROR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION); //0x0506 given when the attempted framebuffer is not complete (https://www.khronos.org/opengl/wiki/Framebuffer_Object#Framebuffer_Completeness)
              break;
              //GLenum glCheckFramebufferStatus(GLenum TARGET);
            GL_ERROR_CASE(GL_OUT_OF_MEMORY); //0x0505
              break;
            default: snprintf(error, sizeof(error), "%s", "UNKNOWN_ERROR"); break;
        }

        fprintf(stderr, "%s - %s: %d\n", error, file, line);
    }
}

void errorCallback(int error, const char* description){
  fprintf(stderr, "Error: %s\n", description);
}

#undef GL_ERROR_CASE

//buffer code can go here
struct Buffer{
  size_t width, height;
  uint32_t* data;
};
uint32_t rgbTranslate(uint8_t r, uint8_t g, uint8_t b){
  return (r << 24) | (g << 16) | (b << 8) | 255;
  //r will separate bits 24->16 as red, 16->8 as green, and 8->1 as blue
}
void clearBuffer(Buffer* buffer, uint32_t color){ //we have to clear the buffer after a process
  for(size_t i=0; i<buffer->width * buffer->height; i++){ //for each piece of memory in the buffer,
    buffer->data[i] = color; //replace the pixels with the set color
  }
}

void validateShader(GLuint shader, const char* file=0){
  static const unsigned int BUFFER_SIZE=512;
  char buffer[BUFFER_SIZE];
  GLsizei length=0;

  glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);

  if(length>0){ //probably need a fail case for less than
    printf("Shader %d(%s) compile error: %s\n", shader, (file ? file: ""), buffer);
  }
}
bool vaidate_program(GLuint program){
  static const GLsizei BUFFER_SIZE=512;
  GLchar buffer[BUFFER_SIZE];
  GLsizei length=0;
  glGetProgramInfoLog(program, BUFFER_SIZE, &length, buffer);
  if (length>0){
    printf("Program %d link error: %s\n", program, buffer);
    return false;
  }
  return true;
}

int main(int argc, char* argv[]){
    const size_t bufferWidth=224;
    const size_t bufferHeight=256;

    glfwSetErrorCallback(errorCallback);

    GLFWwindow* window;

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Space Invaders", NULL, NULL);
    if(!window){
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    GLenum err = glewInit();
    if(err != GLEW_OK){
        fprintf(stderr, "Error initializing GLEW.\n");
        glfwTerminate();
        return -1;
    }
    int glVersion[2] = {-1, 1};
    glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
    glGetIntegerv(GL_MINOR_VERSION, &glVersion[1]);

    gl_debug(__FILE__, __LINE__);

    printf("Using OpenGL: %d.%d\n", glVersion[0], glVersion[1]);
    printf("Renderer used: %s\n", glGetString(GL_RENDERER));
    printf("Shading Language: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    glClearColor(1.0, 0.0, 0.0, 1.0);

    GLuint fullscreen_triangle_vao;
    glGenVertexArrays(1, &fullscreen_triangle_vao);
    glBindVertexArray(fullscreen_triangle_vao);

    //create the fragment shader before vertex, (large comes before small)
    static const char* fragment_shader =
        "\n"
        "#version 330\n"
        "\n"
        "uniform sampler2D buffer;\n"
        "noperspective in vec2 TexCoord;\n"
        "\n"
        "out vec3 outColor;\n"
        "\n"
        "void main(void){\n"
        "    outColor = texture(buffer, TexCoord).rgb;\n"
        "}\n";
    static const char* vertex_shader =
        "\n"
        "#version 330\n"
        "\n"
        "noperspective out vec2 TexCoord;\n"
        "\n"
        "void main(void){\n"
        "\n"
        "    TexCoord.x = (gl_VertexID == 2)? 2.0: 0.0;\n"
        "    TexCoord.y = (gl_VertexID == 1)? 2.0: 0.0;\n"
        "    \n"
        "    gl_Position = vec4(2.0 * TexCoord - 1.0, 0.0, 1.0);\n"
        "}\n";

    GLuint shader_id = glCreateProgram();
    //create the vertex shader
    {
      GLuint shader_vp = glCreateShader(GL_VERTEX_SHADER);

      glShaderSource(shader_vp, 1, &vertex_shader, 0);
      glCompileShader(shader_vp);
      validateShader(shader_vp, vertex_shader);
      glAttachShader(shader_id, shader_vp);

      glDeleteShader(shader_vp);
    }
    //create the fragment shader
    {
      GLuint shader_fp = glCreateShader(GL_FRAGMENT_SHADER);

      glShaderSource(shader_fp, 1, &fragment_shader, 0);
      glCompileShader(shader_fp);
      validateShader(shader_fp, fragment_shader);
      glAttachShader(shader_id, shader_fp);

      glDeleteShader(shader_fp);
    }


    uint32_t clearColor = rgbTranslate(0, 128, 0);

    Buffer buffer;
    buffer.width=bufferWidth;
    buffer.height=bufferHeight;
    buffer.data=new uint32_t[buffer.width * buffer.height];
    clearBuffer(&buffer, clearColor);

    while (!glfwWindowShouldClose(window)){
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
