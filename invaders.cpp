//Compile using: g++ -lglfw -lGLEW -lGL invaders.cpp -o invaders

#include <cstdio>
#include <cstdint>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

bool running = false;
int moveDirection = 0;
bool fire = 0;

#define GL_ERROR_CASE(glerror)\
    case glerror: snprintf(error, sizeof(error), "%s", #glerror)

inline void gl_debug(const char *file, int line) {
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
#undef GL_ERROR_CASE

void error_callback(int error, const char* description){
  fprintf(stderr, "Error: %s\n", description);
}

//These two functions allow the shader constructor to run properly
//glLinkProgram links the two shaders to create the vertex array object (vao)
void validate_shader(GLuint shader, const char *file = 0){
    static const unsigned int BUFFER_SIZE = 512;
    char buffer[BUFFER_SIZE];
    GLsizei length = 0;

    glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);

    if(length>0){
        printf("Shader %d(%s) compile error: %s\n", shader, (file? file: ""), buffer);
    }
}

bool validate_program(GLuint program){
    static const GLsizei BUFFER_SIZE = 512;
    GLchar buffer[BUFFER_SIZE];
    GLsizei length = 0;

    glGetProgramInfoLog(program, BUFFER_SIZE, &length, buffer);

    if(length>0){
        printf("Program %d link error: %s\n", program, buffer);
        return false;
    }

    return true;
}

//mods are modifier keys, scancode is the signature to tell which key has been pressed
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    switch(key){
    case GLFW_KEY_ESCAPE:
        if(action == GLFW_PRESS) running = false;
        break;
    case GLFW_KEY_RIGHT:
        if(action == GLFW_PRESS) moveDirection += 1;
        else if(action == GLFW_RELEASE) moveDirection -= 1;
        break;
    case GLFW_KEY_LEFT:
        if(action == GLFW_PRESS) moveDirection -= 1;
        else if(action == GLFW_RELEASE) moveDirection += 1;
        break;
    case GLFW_KEY_SPACE:
        if(action == GLFW_RELEASE) fire = true;
        break;
    default:
        break;
    }
}

struct Buffer
{
  size_t width, height;
  uint32_t* data;
};

struct Sprite
{
  size_t width, height;
  uint8_t* data;
};

struct Alien
{
  size_t x, y;
  uint8_t type;
};

struct Bullet
{
  size_t x, y;
  int dir;
};

struct Player
{
  size_t x, y;
  size_t life;
};

#define GAME_MAX_BULLETS 128
struct Game
{
  size_t width, height;
  size_t num_aliens;
  size_t num_bullets;
  Alien* aliens;
  Player player;
  Bullet bullets[GAME_MAX_BULLETS];
};

struct SpriteAnimation
{
  bool loop;
  size_t num_frames;
  size_t frame_duration;
  size_t time;
  Sprite** frames;
};

enum AlienType: uint8_t
{
  typeDead = 0,
  typeA = 1,
  typeB = 2,
  typeC = 3
};

//BUFFER CODE CAN GO HERE
//translates rgb color coding to uint_32 type
uint32_t rgbTranslate(uint8_t r, uint8_t g, uint8_t b){
  return (r << 24) | (g << 16) | (b << 8) | 255;
  //r will separate bits 24->16 as red, 16->8 as green, and 8->1 as blue
}

void buffer_clear(Buffer* buffer, uint32_t color){ //we have to clear the buffer after a process
  for(size_t i = 0; i < buffer->width * buffer->height; i++){ //for each piece of memory in the buffer,
    buffer->data[i] = color; //replace the pixels with the set color
  }
}

//For simplicity and speed this will just check for hitbox overlap
bool sprite_overlap_check(const Sprite& spriteA, size_t x_a, size_t y_a,
                          const Sprite& spriteB, size_t x_b, size_t y_b){
  if(x_a < x_b + spriteB.width && x_a + spriteA.width > x_b &&
     y_a < y_b + spriteB.height && y_a + spriteA.height > y_b)
  {
    return true;
  }
  return false;
}

//We draw the sprites in the buffer by filling a 2D array with bitmapping: all the data is going into the sprite struct
void buffer_draw_sprite(Buffer* buffer, const Sprite& sprite, size_t x, size_t y, uint32_t color){
    for(size_t xi = 0; xi < sprite.width; xi++)
    {
        for(size_t yi = 0; yi < sprite.height; yi++)
        {
            if(sprite.data[yi * sprite.width + xi] &&
               (sprite.height - 1 + y - yi) < buffer->height &&
               (x + xi) < buffer->width)
            {
                buffer->data[(sprite.height - 1 + y - yi) * buffer->width + (x + xi)] = color;
            }
        }
    }
}

//MAIN FUNCTION
int main(int argc, char* argv[])
{
    const size_t bufferWidth = 224;
    const size_t bufferHeight = 256;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    //Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(2 * bufferWidth, 2 * bufferHeight, "Space Invaders", NULL, NULL);
    if(!window){
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(window, key_callback);

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

    glfwSwapInterval(1);

    glClearColor(1.0, 0.0, 0.0, 1.0);

    // Create graphics buffer
    Buffer buffer;
    buffer.width  = bufferWidth;
    buffer.height = bufferHeight;
    buffer.data   = new uint32_t[buffer.width * buffer.height];

    buffer_clear(&buffer, 0);

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
        validate_shader(shader_vp, vertex_shader);
        glAttachShader(shader_id, shader_vp);

        glDeleteShader(shader_vp);
    }
    {
        //Create fragment shader
        GLuint shader_fp = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shader_fp, 1, &fragment_shader, 0);
        glCompileShader(shader_fp);
        validate_shader(shader_fp, fragment_shader);
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

    // Prepare game sprites (height, width, bitmapping data)
    Sprite alien_sprites[6];

    alien_sprites[0].width = 8;
    alien_sprites[0].height = 8;
    alien_sprites[0].data = new uint8_t[64]
    {
        0,0,0,1,1,0,0,0, // ...@@...
        0,0,1,1,1,1,0,0, // ..@@@@..
        0,1,1,1,1,1,1,0, // .@@@@@@.
        1,1,0,1,1,0,1,1, // @@.@@.@@
        1,1,1,1,1,1,1,1, // @@@@@@@@
        0,1,0,1,1,0,1,0, // .@.@@.@.
        1,0,0,0,0,0,0,1, // @......@
        0,1,0,0,0,0,1,0  // .@....@.
    };

    alien_sprites[1].width = 8;
    alien_sprites[1].height = 8;
    alien_sprites[1].data = new uint8_t[64]
    {
        0,0,0,1,1,0,0,0, // ...@@...
        0,0,1,1,1,1,0,0, // ..@@@@..
        0,1,1,1,1,1,1,0, // .@@@@@@.
        1,1,0,1,1,0,1,1, // @@.@@.@@
        1,1,1,1,1,1,1,1, // @@@@@@@@
        0,0,1,0,0,1,0,0, // ..@..@..
        0,1,0,1,1,0,1,0, // .@.@@.@.
        1,0,1,0,0,1,0,1  // @.@..@.@
    };

    alien_sprites[2].width = 11;
    alien_sprites[2].height = 8;
    alien_sprites[2].data = new uint8_t[88]
    {
        0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
        0,0,0,1,0,0,0,1,0,0,0, // ...@...@...
        0,0,1,1,1,1,1,1,1,0,0, // ..@@@@@@@..
        0,1,1,0,1,1,1,0,1,1,0, // .@@.@@@.@@.
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
        1,0,1,0,0,0,0,0,1,0,1, // @.@.....@.@
        0,0,0,1,1,0,1,1,0,0,0  // ...@@.@@...
    };

    alien_sprites[3].width = 11;
    alien_sprites[3].height = 8;
    alien_sprites[3].data = new uint8_t[88]
    {
        0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
        1,0,0,1,0,0,0,1,0,0,1, // @..@...@..@
        1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
        1,1,1,0,1,1,1,0,1,1,1, // @@@.@@@.@@@
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
        0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
        0,1,0,0,0,0,0,0,0,1,0  // .@.......@.
    };

    alien_sprites[4].width = 12;
    alien_sprites[4].height = 8;
    alien_sprites[4].data = new uint8_t[96]
    {
        0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
        0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        0,0,0,1,1,0,0,1,1,0,0,0, // ...@@..@@...
        0,0,1,1,0,1,1,0,1,1,0,0, // ..@@.@@.@@..
        1,1,0,0,0,0,0,0,0,0,1,1  // @@........@@
    };


    alien_sprites[5].width = 12;
    alien_sprites[5].height = 8;
    alien_sprites[5].data = new uint8_t[96]
    {
        0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
        0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        0,0,1,1,1,0,0,1,1,1,0,0, // ..@@@..@@@..
        0,1,1,0,0,1,1,0,0,1,1,0, // .@@..@@..@@.
        0,0,1,1,0,0,0,0,1,1,0,0  // ..@@....@@..
    };

    Sprite alien_death_sprite;
    alien_death_sprite.width = 13;
    alien_death_sprite.height = 7;
    alien_death_sprite.data = new uint8_t[91]
    {
        0,1,0,0,1,0,0,0,1,0,0,1,0, // .@..@...@..@.
        0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
        0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
        1,1,0,0,0,0,0,0,0,0,0,1,1, // @@.........@@
        0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
        0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
        0,1,0,0,1,0,0,0,1,0,0,1,0  // .@..@...@..@.
    };

    Sprite player_sprite;
    player_sprite.width = 11;
    player_sprite.height = 7;
    player_sprite.data = new uint8_t[77]
    {
        0,0,0,0,0,1,0,0,0,0,0, // .....@.....
        0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
        0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
        0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
    };

    Sprite bullet_sprite;
    bullet_sprite.width = 1;
    bullet_sprite.height = 3;
    bullet_sprite.data = new uint8_t[3]
    {
        1,
        1,
        1
    };

    SpriteAnimation alienAnimation[3];

    for(size_t i = 0; i < 3; i++)
    {
        alienAnimation[i].loop = true;
        alienAnimation[i].num_frames = 2;
        alienAnimation[i].frame_duration = 10;
        alienAnimation[i].time = 0;

        alienAnimation[i].frames = new Sprite*[2];
        alienAnimation[i].frames[0] = &alien_sprites[2 * i];
        alienAnimation[i].frames[1] = &alien_sprites[2 * i + 1];
    }

    Game game;
    game.width = bufferWidth;
    game.height = bufferHeight;
    game.num_bullets = 0;
    game.num_aliens = 55;
    game.aliens = new Alien[game.num_aliens];

    //player position starts at the bottom row in the center
    game.player.x = 112 - 5;
    game.player.y = 32;

    game.player.life = 3;

    for(size_t yi = 0; yi < 5; yi++){
      for(size_t xi = 0; xi < 11; xi++){
        Alien& alien = game.aliens[yi * 11 + xi];
        alien.type = (5 - yi) / 2 + 1;

        const Sprite& sprite = alien_sprites[2 * (alien.type - 1)];

        alien.x = 16 * xi + 20 + (alien_death_sprite.width - sprite.width)/2;
        alien.y = 17 * yi + 128;
      }
    }

    uint8_t* deathCounters = new uint8_t[game.num_aliens];
    for(size_t i = 0; i < game.num_aliens; i++){
        deathCounters[i] = 10;
    }

    uint32_t clear_color = rgbTranslate(0, 128, 0);

    running = true;
    int playerDirection = 0;
    size_t score = 0;

    //MAIN GAME LOOP
    while (!glfwWindowShouldClose(window) && running){
        buffer_clear(&buffer, clear_color);

        // Draw all sprites using our function
        for(size_t ai = 0; ai < game.num_aliens; ai++){
          if(!deathCounters[ai]) continue;

          const Alien& alien = game.aliens[ai];
          if(alien.type == typeDead){
            buffer_draw_sprite(&buffer, alien_death_sprite, alien.x, alien.y, rgbTranslate(128, 0, 0));
          }
          else{
            const SpriteAnimation& animation = alienAnimation[alien.type - 1];
            size_t current_frame = animation.time / animation.frame_duration;
            const Sprite& sprite = *animation.frames[current_frame];
            buffer_draw_sprite(&buffer, sprite, alien.x, alien.y, rgbTranslate(128, 0, 0));
          }
        }

        for(size_t bi = 0; bi < game.num_bullets; bi++){
            const Bullet& bullet = game.bullets[bi];
            const Sprite& sprite = bullet_sprite;
            buffer_draw_sprite(&buffer, sprite, bullet.x, bullet.y, rgbTranslate(128, 0, 0));
        }

        buffer_draw_sprite(&buffer, player_sprite, game.player.x, game.player.y, rgbTranslate(128, 0, 0));

        // Update animations
        for(size_t i = 0; i < 3; i++){
          ++alienAnimation[i].time;
          if(alienAnimation[i].time == alienAnimation[i].num_frames * alienAnimation[i].frame_duration){
            alienAnimation[i].time = 0;
          }
        }

        glTexSubImage2D(
          GL_TEXTURE_2D, 0, 0, 0,
          buffer.width, buffer.height,
          GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
          buffer.data
        );
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);

        // Simulate aliens
        for(size_t ai = 0; ai < game.num_aliens; ai++){
          const Alien& alien = game.aliens[ai];
          if(alien.type == typeDead && deathCounters[ai]){
            deathCounters[ai]--;
          }
        }

        // Simulate bullets
        for(size_t bi = 0; bi < game.num_bullets; bi++){
            game.bullets[bi].y += game.bullets[bi].dir;
            if(game.bullets[bi].y >= game.height || game.bullets[bi].y < bullet_sprite.height){
                game.bullets[bi] = game.bullets[game.num_bullets - 1];
                -game.num_bullets;
                continue;
            }
            // Check hit
            for(size_t ai = 0; ai < game.num_aliens; ai++)
            {
                const Alien& alien = game.aliens[ai];
                if(alien.type == typeDead) continue;


                const SpriteAnimation& animation = alienAnimation[alien.type - 1];
                size_t current_frame = animation.time / animation.frame_duration;
                const Sprite& alien_sprite = *animation.frames[current_frame];
                bool overlap = sprite_overlap_check(
                    bullet_sprite, game.bullets[bi].x, game.bullets[bi].y,
                    alien_sprite, alien.x, alien.y
                );
                if(overlap)
                {
                    game.aliens[ai].type = typeDead;

                    // NOTE: Hack to recenter death sprite
                    game.aliens[ai].x -= (alien_death_sprite.width - alien_sprite.width)/2;
                    game.bullets[bi] = game.bullets[game.num_bullets - 1];
                    game.num_bullets--;
                    continue;
                }
            }
        }

        // Simulate player
        playerDirection = 2 * moveDirection;

        if(playerDirection != 0)
        {
            if(game.player.x + player_sprite.width + playerDirection >= game.width)
            {
                game.player.x = game.width - player_sprite.width;
            }
            else if((int)game.player.x + playerDirection <= 0)
            {
                game.player.x = 0;
            }
            else game.player.x += playerDirection;
        }

        // Process events
        if(fire && game.num_bullets < GAME_MAX_BULLETS)
        {
            game.bullets[game.num_bullets].x = game.player.x + player_sprite.width / 2;
            game.bullets[game.num_bullets].y = game.player.y + player_sprite.height;
            game.bullets[game.num_bullets].dir = 2;
            game.num_bullets++;
        }
        fire = false;

        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    glDeleteVertexArrays(1, &fullscreen_triangle_vao);

    for(size_t i = 0; i < 6; i++)
    {
        delete[] alien_sprites[i].data;
    }

    delete[] alien_death_sprite.data;

    for(size_t i = 0; i < 3; i++)
    {
        delete[] alienAnimation[i].frames;
    }
    delete[] buffer.data;
    delete[] game.aliens;
    delete[] deathCounters;

    return 0;
}
