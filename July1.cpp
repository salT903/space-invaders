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

//sprite code can go here
struct Sprite{
    size_t width, height;
    uint8_t* data;
};
bool sprite_overlap_check(
  const Sprite& sp_a, size_t x_a, size_t y_a,
  const Sprite& sp_b, size_t x_b, size_t y_b
)
{
  // NOTE: For simplicity we just check for overlap of the sprite
  // rectangles. Instead, if the rectangles overlap, we should
  // further check if any pixel of sprite A overlap with any of
  // sprite B.
  if(x_a < x_b + sp_b.width && x_a + sp_a.width > x_b && y_a < y_b + sp_b.height && y_a + sp_a.height > y_b){
      return true;
  }
  return false;
}

void buffer_draw_sprite(Buffer* buffer, const Sprite& sprite, size_t x, size_t y, uint32_t color){
  for(size_t xi = 0; xi < sprite.width; ++xi){
    for(size_t yi = 0; yi < sprite.height; ++yi){
      if(sprite.data[yi * sprite.width + xi] &&
       (sprite.height - 1 + y - yi) < buffer->height &&
       (x + xi) < buffer->width){
        buffer->data[(sprite.height - 1 + y - yi) * buffer->width + (x + xi)] = color;
      }
    }
  }
}

//These two functions allow the shader constructor to run properly
//glLinkProgram links the two shaders to create the vertex array object (vao)
void validateShader(GLuint shader, const char* file=0){
  static const unsigned int BUFFER_SIZE=512;
  char buffer[BUFFER_SIZE];
  GLsizei length=0;

  glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);

  if(length>0){ //probably need a fail case for less than
    printf("Shader %d(%s) compile error: %s\n", shader, (file ? file: ""), buffer);
  }
}
bool validate_program(GLuint program){
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
  const size_t buffer_width=224;
  const size_t buffer_height=256;

  glfwSetErrorCallback(errorCallback);

  if (!glfwInit()) return -1;

  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  /* Create a windowed mode window and its OpenGL context */
  GLFWwindow* window = glfwCreateWindow(buffer_width, buffer_height, "Space Invaders", NULL, NULL);
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

    // Create graphics buffer
  Buffer buffer;
  buffer.width  = buffer_width;
  buffer.height = buffer_height;
  buffer.data   = new uint32_t[buffer.width * buffer.height];

  clearBuffer(&buffer, 0);

  // Create texture for presenting buffer to OpenGL
  GLuint buffer_texture;
  glGenTextures(1, &buffer_texture);
  glBindTexture(GL_TEXTURE_2D, buffer_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, buffer.width, buffer.height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


  // Create vao for generating fullscreen triangle
  GLuint fullscreen_triangle_vao;
  glGenVertexArrays(1, &fullscreen_triangle_vao);


  // Create shader for displaying buffer
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
  {
    //Create vertex shader
    GLuint shader_vp = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader_vp, 1, &vertex_shader, 0);
    glCompileShader(shader_vp);
    validateShader(shader_vp, vertex_shader);
    glAttachShader(shader_id, shader_vp);

    glDeleteShader(shader_vp);
  }

  {
    //Create fragment shader
    GLuint shader_fp = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader_fp, 1, &fragment_shader, 0);
    glCompileShader(shader_fp);
    validateShader(shader_fp, fragment_shader);
    glAttachShader(shader_id, shader_fp);

    glDeleteShader(shader_fp);
  }

  glLinkProgram(shader_id);

  if(!validate_program(shader_id)){
    fprintf(stderr, "Error while validating shader.\n");
    glfwTerminate();
    glDeleteVertexArrays(1, &fullscreen_triangle_vao);
    delete[] buffer.data;
    return -1;
  }

  glUseProgram(shader_id);
  GLint location = glGetUniformLocation(shader_id, "buffer");
  glUniform1i(location, 0);


  //OpenGL setup
  glDisable(GL_DEPTH_TEST);
  glActiveTexture(GL_TEXTURE0);

  glBindVertexArray(fullscreen_triangle_vao);

  // Prepare game
  Sprite alien_sprite;
  alien_sprite.width = 11;
  alien_sprite.height = 8;
  alien_sprite.data = new uint8_t[88]{
    0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
    0,0,0,1,0,0,0,1,0,0,0, // ...@...@...
    0,0,1,1,1,1,1,1,1,0,0, // ..@@@@@@@..
    0,1,1,0,1,1,1,0,1,1,0, // .@@.@@@.@@.
    1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
    1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
    1,0,1,0,0,0,0,0,1,0,1, // @.@.....@.@
    0,0,0,1,1,0,1,1,0,0,0  // ...@@.@@...
  };

  uint32_t clear_color = rgbTranslate(0, 128, 0);

  while (!glfwWindowShouldClose(window)){
    clearBuffer(&buffer, clear_color);

    buffer_draw_sprite(&buffer, alien_sprite, 112, 128, rgbTranslate(128, 0, 0));

    glTexSubImage2D(
      GL_TEXTURE_2D, 0, 0, 0,
      buffer.width, buffer.height,
      GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
      buffer.data
    );
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  glDeleteVertexArrays(1, &fullscreen_triangle_vao);

  delete[] alien_sprite.data;
  delete[] buffer.data;

  return 0;
}
