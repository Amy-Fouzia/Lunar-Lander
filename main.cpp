/**
* Author: Amy Fouzia
* Assignment: Lunar Lander
* Date due: 2023-11-08, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define LOG(argument) std::cout << argument << '\n'
#define STB_IMAGE_IMPLEMENTATION
#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#define NUMBER_OF_ENEMIES 3
#define FIXED_TIMESTEP 0.0166666f
#define ACC_OF_GRAVITY -1.0f
#define PLATFORM_COUNT 11

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"

// ————— STRUCTS AND ENUMS —————//
struct GameState
{
    Entity* player;
    Entity* platforms;
};

// ————— CONSTANTS ————— //
const int WINDOW_WIDTH = 640,
WINDOW_HEIGHT = 480;

const float BG_RED = 0.1922f,
BG_BLUE = 0.549f,
BG_GREEN = 0.9059f,
BG_OPACITY = 1.0f;

const int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";


const float MILLISECONDS_IN_SECOND = 1000.0;
const char  SPRITESHEET_FILEPATH[] = "assets/huh.png",
PLATFORM1_FILEPATH[] = "assets/land_platform.png",
PLATFORM2_FILEPATH[] = "assets/water_platform.png",
SUCCESS_FILEPATH[] = "assets/success.png",
FAIL_FILEPATH[] = "assets/fail.png";

const int NUMBER_OF_TEXTURES = 1;  // to be generated, that is
const GLint LEVEL_OF_DETAIL = 0;  // base image level; Level n is the nth mipmap reduction image
const GLint TEXTURE_BORDER = 0;  // this value MUST be zero

// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
bool g_game_is_running = true,
     game_end = false;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;
float cat_food = 5.0f; //fuel

// ———— GENERAL FUNCTIONS ———— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("Lunar Lander",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif

    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    // ————— PLAYER ————— //
    g_game_state.player = new Entity();
    g_game_state.player->set_position(glm::vec3(-4.5f, 3.0f, 0.0f));
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_game_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.1, 0.0f));
    g_game_state.player->set_speed(1.0f);
    g_game_state.player->m_texture_id = load_texture(SPRITESHEET_FILEPATH);

    g_game_state.player->m_walking[g_game_state.player->LEFT] = new int[4] { 1, 5, 9, 13 };
    g_game_state.player->m_walking[g_game_state.player->RIGHT] = new int[4] { 3, 7, 11, 15 };
    g_game_state.player->m_walking[g_game_state.player->UP] = new int[4] { 2, 6, 10, 14 };
    g_game_state.player->m_walking[g_game_state.player->DOWN] = new int[4] { 0, 4, 8, 12 };

    g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];  // start George looking right
    g_game_state.player->m_animation_frames = 4;
    g_game_state.player->m_animation_index = 0;
    g_game_state.player->m_animation_time = 0.0f;
    g_game_state.player->m_animation_cols = 4;
    g_game_state.player->m_animation_rows = 4;

    // ————— PLATFORM ————— //
    g_game_state.platforms = new Entity[PLATFORM_COUNT];

    for (int i = 0; i < PLATFORM_COUNT; i++)
    {
        if (i % 3 == 0) {
            g_game_state.platforms[i].m_texture_id = load_texture(PLATFORM1_FILEPATH);
            g_game_state.platforms[i].set_position(glm::vec3(i - 5.0f, -3.0f, 0.0f));
            g_game_state.platforms[i].update(0.0f, NULL, 0);
        }
        else {
            g_game_state.platforms[i].m_texture_id = load_texture(PLATFORM2_FILEPATH);
            g_game_state.platforms[i].set_position(glm::vec3(i - 5.0f, -3.0f, 0.0f));
            g_game_state.platforms[i].update(0.0f, NULL, 0);
        }
    }

    // ————— GENERAL ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (key_state[SDL_SCANCODE_LEFT] && cat_food > 0)
    {
        g_game_state.player->move_left();  
        g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->LEFT];
    }
    else if (key_state[SDL_SCANCODE_RIGHT] && cat_food > 0)
    {
        g_game_state.player->move_right();
        g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];
    }

    // This makes sure that the player can't move faster diagonally
    if (glm::length(g_game_state.player->get_movement()) > 1.0f)
    {
        g_game_state.player->set_movement(glm::normalize(g_game_state.player->get_movement()));
    }
}

void update()
{
    // ————— DELTA TIME ————— //
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND; // get the current number of ticks
    float delta_time = ticks - g_previous_ticks; // the delta time is the difference from the last frame
    g_previous_ticks = ticks;

    if (g_game_state.player->get_acceleration()[0] != 0) {
        cat_food += -1.0f * delta_time;
    }

    std::cout << "cat_food: ";
    std::cout << cat_food;
    std::cout << "\n";

    //std::cout << "acceleration x: ";
    //std::cout << g_game_state.player->get_acceleration()[0];
    //std::cout << "\n";

    //std::cout << "position x: ";
    //std::cout << g_game_state.player->get_position()[0];
    //std::cout << "\n";

    //if not moving, slowly make horizontal acceleration 0 again
    for (int i = 9; i >= 0; i--) {
        if (g_game_state.player->get_acceleration()[0] > 0) {
            g_game_state.player->sub_acceleration_x(0.01 * delta_time);
            if (g_game_state.player->get_acceleration()[0] < 0) {
                g_game_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.1, 0.0f));
            }
        }

        if (g_game_state.player->get_acceleration()[0] < 0) {
            g_game_state.player->sub_acceleration_x(-0.01 * delta_time);
            if (g_game_state.player->get_acceleration()[0] > 0) {
                g_game_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.1, 0.0f));
            }
        }
    }

    //collision with platforms
    if (g_game_state.player->m_collided_bottom) {
            if (g_game_state.player->get_position()[0] <= -4.7) {
                //win
                g_game_state.platforms[PLATFORM_COUNT / 2].m_texture_id = load_texture(SUCCESS_FILEPATH);
                cat_food = 0;
                g_game_state.player->set_movement(glm::vec3(0.0f));
                g_game_state.player->set_acceleration(glm::vec3(0.0f));
    }
            else if (g_game_state.player->get_position()[0] >= -2.3 && g_game_state.player->get_position()[0] <= -1.7) {
                //win
                g_game_state.platforms[PLATFORM_COUNT / 2].m_texture_id = load_texture(SUCCESS_FILEPATH);
                cat_food = 0;
                g_game_state.player->set_movement(glm::vec3(0.0f));
                g_game_state.player->set_acceleration(glm::vec3(0.0f));
            }
            else if (g_game_state.player->get_position()[0] >= 0.7 && g_game_state.player->get_position()[0] <= 1.3) {
                //win
                g_game_state.platforms[PLATFORM_COUNT / 2].m_texture_id = load_texture(SUCCESS_FILEPATH);
                cat_food = 0;
                g_game_state.player->set_movement(glm::vec3(0.0f));
                g_game_state.player->set_acceleration(glm::vec3(0.0f));
            }
            else if (g_game_state.player->get_position()[0] >= 3.7 && g_game_state.player->get_position()[0] <= 4.3) {
                //win
                g_game_state.platforms[PLATFORM_COUNT / 2].m_texture_id = load_texture(SUCCESS_FILEPATH);
                cat_food = 0;
                g_game_state.player->set_movement(glm::vec3(0.0f));
                g_game_state.player->set_acceleration(glm::vec3(0.0f));
            }
            else {
                //lose
                g_game_state.platforms[PLATFORM_COUNT / 2].m_texture_id = load_texture(FAIL_FILEPATH);
                cat_food = 0;
                g_game_state.player->set_movement(glm::vec3(0.0f));
                g_game_state.player->set_acceleration(glm::vec3(0.0f));
            }
    }

    // ————— FIXED TIMESTEP ————— //
    // STEP 1: Keep track of how much time has passed since last step
    delta_time += g_time_accumulator;

    // STEP 2: Accumulate the ammount of time passed while we're under our fixed timestep
    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }

    // STEP 3: Once we exceed our fixed timestep, apply that elapsed time into the objects' update function invocation
    while (delta_time >= FIXED_TIMESTEP)
    {
        // Notice that we're using FIXED_TIMESTEP as our delta time
        g_game_state.player->update(FIXED_TIMESTEP, g_game_state.platforms, PLATFORM_COUNT);
        delta_time -= FIXED_TIMESTEP;
    }

    g_time_accumulator = delta_time;

    //collision wiith right/left walls (loss)
    if (g_game_state.player->get_position()[0] > 4.875)
    {
        g_game_state.platforms[PLATFORM_COUNT / 2].m_texture_id = load_texture(FAIL_FILEPATH);
        cat_food = 0;
        g_game_state.player->set_movement(glm::vec3(0.0f));
        g_game_state.player->set_acceleration(glm::vec3(0.0f));
    }
    if (g_game_state.player->get_position()[0] < -4.875)
    {
        g_game_state.platforms[PLATFORM_COUNT / 2].m_texture_id = load_texture(FAIL_FILEPATH);
        cat_food = 0;
        g_game_state.player->set_movement(glm::vec3(0.0f));
        g_game_state.player->set_acceleration(glm::vec3(0.0f));
    }
}

void render()
{
    // ————— GENERAL ————— //
    glClear(GL_COLOR_BUFFER_BIT);

    // ————— PLAYER ————— //
    g_game_state.player->render(&g_shader_program);

    // ————— PLATFORM ————— //
    for (int i = 0; i < PLATFORM_COUNT; i++) g_game_state.platforms[i].render(&g_shader_program);

    // ————— GENERAL ————— //
    SDL_GL_SwapWindow(g_display_window);
}

void shutdown() { SDL_Quit(); }

// ————— DRIVER GAME LOOP ————— /
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}