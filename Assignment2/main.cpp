#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#define GL_GLEXT_PROTOTYPES 1
#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>

#define LOG(argument) std::cout << argument << '\n'

const int WINDOW_WIDTH  = 640,
          WINDOW_HEIGHT = 480;

const float BG_RED     = 1.0f,
            BG_BLUE    = 1.0f,
            BG_GREEN   = 1.0f,
            BG_OPACITY = 1.0f;

const int VIEWPORT_X = 0,
          VIEWPORT_Y = 0,
          VIEWPORT_WIDTH  = WINDOW_WIDTH,
          VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
           F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;

const char PLAYER_SPRITE_FILEPATH[] = "bar.png";
const char PLAYER_SPRITE_FILEPATH2[] = "ball.png";

SDL_Window* display_window;
bool game_is_running = true;
bool is_growing = true;

ShaderProgram program;
glm::mat4 view_matrix, model_matrix, projection_matrix, trans_matrix, other_model_matrix, pong_model_matrix;

float previous_ticks = 0.0f;
float pong_width = 0.5f;
float pong_height = 0.5f;
float right_block_width = 0.5f;
float right_block_height = 2.0f;
float left_block_width = 0.5f;
float left_block_height = 2.0f;

GLuint player_texture_id;
GLuint other_texture_id;
GLuint pong_texture_id;

glm::vec3 right_block_position = glm::vec3(5.0f, 0.0f, 0.0f);
glm::vec3 right_movement = glm::vec3(0.0f, 0.0f, 0.0f);

glm::vec3 left_block_position = glm::vec3(-5.0f, 0.0f, 0.0f);
glm::vec3 left_movement = glm::vec3(0.0f, 0.0f, 0.0f);

glm::vec3 pong_position = glm::vec3(-3.0f, 0.0f, 0.0f);
glm::vec3 pong_movement = glm::vec3(1.0f, 1.0f, 0.0f);

float player_speed = 4.0f;  // move 1 unit per second

#define LOG(argument) std::cout << argument << '\n'

const int NUMBER_OF_TEXTURES = 1; // to be generated, that is
const GLint LEVEL_OF_DETAIL  = 0;  // base image level; Level n is the nth mipmap reduction image
const GLint TEXTURE_BORDER   = 0;   // this value MUST be zero

GLuint load_texture(const char* filepath)
{
    // STEP 1: Loading the image file
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);
    
    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }
    
    // STEP 2: Generating and binding a texture ID to our image
    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);
    
    // STEP 3: Setting our texture filter parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // STEP 4: Releasing our file from memory and returning our texture id
    stbi_image_free(image);
    
    return textureID;
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    display_window = SDL_CreateWindow("Ping, Pong!",
                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      WINDOW_WIDTH, WINDOW_HEIGHT,
                                      SDL_WINDOW_OPENGL);
    
    SDL_GLContext context = SDL_GL_CreateContext(display_window);
    SDL_GL_MakeCurrent(display_window, context);
    
#ifdef _WINDOWS
    glewInit();
#endif
    
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    
    program.Load(V_SHADER_PATH, F_SHADER_PATH);
    
    model_matrix = glm::mat4(1.0f);
    other_model_matrix = glm::mat4(1.0f);
    pong_model_matrix = glm::mat4(1.0f);
    
    view_matrix = glm::mat4(1.0f);  // Defines the position (location and orientation) of the camera
    projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);  // Defines the characteristics of your camera, such as clip planes, field of view, projection method etc.
    
    program.SetProjectionMatrix(projection_matrix);
    program.SetViewMatrix(view_matrix);
    
    glUseProgram(program.programID);
    
    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);
    
    player_texture_id = load_texture(PLAYER_SPRITE_FILEPATH);
    other_texture_id = load_texture(PLAYER_SPRITE_FILEPATH);
    pong_texture_id = load_texture(PLAYER_SPRITE_FILEPATH2);
    
    // enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    // VERY IMPORTANT: If nothing is pressed, we don't want to go anywhere
    right_movement = glm::vec3(0.0f);
    left_movement = glm::vec3(0.0f);
    
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
            case SDL_QUIT:
            case SDL_WINDOWEVENT_CLOSE:
                game_is_running = false;
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                        
                    case SDLK_q:
                        // Quit the game with a keystroke
                        game_is_running = false;
                        break;
                        
                    default:
                        break;
                }
                
            default:
                break;
        }
    }
    
    const Uint8 *key_state = SDL_GetKeyboardState(NULL);

    if (key_state[SDL_SCANCODE_UP])
    {
        //make sure the block cannot go below or above the screen
        if ((right_block_position[1] + right_block_height / 2.0f) > 3.75f)
        {
            right_movement.y = 0.0f;
        } else
        {
            right_movement.y = 1.0f;
        }
    }
    else if (key_state[SDL_SCANCODE_DOWN])
    {
        if ((right_block_position[1] - right_block_height / 2.0f) < -3.75f)
        {
            right_movement.y = 0.0f;
        }
        else
        {
            right_movement.y = -1.0f;
        }
    }
    
    if (key_state[SDL_SCANCODE_W])
    {
        if ((left_block_position[1] + left_block_height / 2.0f) > 3.75f)
        {
            left_movement.y = 0.0f;
        } else
        {
            left_movement.y = 1.0f;
        }
    }
    else if (key_state[SDL_SCANCODE_S])
    {
        if ((left_block_position[1] - left_block_height / 2.0f) < -3.75f)
        {
            left_movement.y = 0.0f;
        } else
        {
            left_movement.y = -1.0f;
        }
    }
    
    // This makes sure that the player can't move faster diagonally
    if (glm::length(right_movement) > 1.0f)
    {
        right_movement = glm::normalize(right_movement);
        left_movement = glm::normalize(left_movement);
    }
}
/**
 RECTANGLE TO RECTANGLE COLLISTION FORMULA
 */
float collision_distance(float a_pos, float b_pos, float length1, float length2){
    return fabs(a_pos - b_pos) - ((length1 + length2) / 2.0f);
}

void update()
{
    //game lost/win condition
    if(pong_position[0] > 6.0f || pong_position[0] < -6.0f){
        game_is_running = false;
    }
    float ticks = (float) SDL_GetTicks() / MILLISECONDS_IN_SECOND; // get the current number of ticks
    float delta_time = ticks - previous_ticks; // the delta time is the difference from the last frame
    previous_ticks = ticks;

    // Add direction * units per second * elapsed time
    right_block_position += right_movement * player_speed * delta_time;
    left_block_position += left_movement * player_speed * delta_time;
    pong_position += pong_movement * player_speed * delta_time;
    
    model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, right_block_position);
    model_matrix = glm::scale(model_matrix, glm::vec3(0.5f, 2.0f, 1.0f));

    other_model_matrix = glm::mat4(1.0f);
    other_model_matrix = glm::translate(other_model_matrix, left_block_position);
    other_model_matrix = glm::scale(other_model_matrix, glm::vec3(0.5f, 2.0f, 1.0f));
    
    pong_model_matrix = glm::mat4(1.0f);
    pong_model_matrix = glm::translate(pong_model_matrix, pong_position);
    pong_model_matrix = glm::scale(pong_model_matrix, glm::vec3(-0.5f, -0.5f, 1.0f));
    
    //pong reflection against the right block
    float x_distance = collision_distance(pong_position[0], right_block_position[0], pong_width, right_block_width);
    float y_distance = collision_distance(pong_position[1], right_block_position[1], pong_height, right_block_height);

    if (x_distance < 0 && y_distance < 0){
        pong_movement[0] = -pong_movement[0];
    }
    
    //pong reflection against the left block
    x_distance = collision_distance(pong_position[0], left_block_position[0], pong_width, left_block_width);
    y_distance = collision_distance(pong_position[1], left_block_position[1], pong_height, left_block_height);
    
    if (x_distance < 0 && y_distance < 0){
        pong_movement[0] = -pong_movement[0];
    }
    
    //pong reflection against ceiling and floors
    if (pong_position[1] - pong_height / 2.0f < -3.75f || pong_position[1] + pong_height / 2.0f > 3.75f){
        pong_movement[1] = -pong_movement[1];
    }
}

void draw_object(glm::mat4 &object_model_matrix, GLuint &object_texture_id)
{
    program.SetModelMatrix(object_model_matrix);
    glBindTexture(GL_TEXTURE_2D, object_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, 6); // we are now drawing 2 triangles, so we use 6 instead of 3
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Vertices
    float vertices[] = {
        -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,  // triangle 1
        -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f   // triangle 2
    };

    // Textures
    float texture_coordinates[] = {
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,     // triangle 1
        0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,     // triangle 2
    };
    
    glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(program.positionAttribute);
    
    glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texture_coordinates);
    glEnableVertexAttribArray(program.texCoordAttribute);
    
    // Bind texture
    draw_object(model_matrix, player_texture_id);
    draw_object(other_model_matrix, other_texture_id);
    draw_object(pong_model_matrix, pong_texture_id);
    
    // We disable two attribute arrays now
    glDisableVertexAttribArray(program.positionAttribute);
    glDisableVertexAttribArray(program.texCoordAttribute);
    
    SDL_GL_SwapWindow(display_window);
}

void shutdown() { SDL_Quit(); }


int main(int argc, char* argv[])
{
    initialise();
    
    while (game_is_running)
    {
        process_input();
        update();
        render();
    }
    
    shutdown();
    return 0;
}
