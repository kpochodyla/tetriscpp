#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <SDL2/SDL.h>


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef float f32;
typedef double f64;

#include "colors.h"

#define WIDTH 10
#define HEIGHT 22
#define VISIT_HEIGHT 20
#define GRID_SIZE 30
#define HIGHLIGHT_TIME 0.5f
#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))


const f32 FRAMES_PER_DROP[] = {
    48,
    43,
    38,
    33,
    28,
    23,
    18,
    14,
    8,
    6,
    5,
    5,
    5,
    4,
    4,
    4,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1
};

const f32 TARGET_SECONDS_PER_FRAME = 1.f / 60.f;

struct Tetrino 
{

    const u8 *data;
    const s32 side;
};

Tetrino tetrino(const u8 *data, s32 side) 
{
    return { data, side };
}

const u8 TETRINO_1[] = {
    0,0,0,0,
    1,1,1,1,
    0,0,0,0,
    0,0,0,0
};

const u8 TETRINO_2[] = {
    2,2,
    2,2
};

const u8 TETRINO_3[] = {
    0, 0, 0,
    3, 3, 3,
    0, 3, 0
};

const u8 TETRINO_4[] = {
    0, 4, 4,
    4, 4, 0,
    0, 0, 0
};

const u8 TETRINO_5[] = {
    5, 5, 0,
    0, 5, 0,
    0, 0, 0,
};

const u8 TETRINO_6[] = {
    6, 0, 0,
    6, 6, 6,
    0, 0, 0
};

const u8 TETRINO_7[] = {
    0, 0, 7,
    7, 7, 7,
    0, 0, 0
};


const Tetrino TETRINOS[] = {
    tetrino(TETRINO_1, 4),
    tetrino(TETRINO_2, 2),
    tetrino(TETRINO_3, 3),
    tetrino(TETRINO_4, 3),
    tetrino(TETRINO_5, 3),
    tetrino(TETRINO_6, 3),
    tetrino(TETRINO_7, 3),
};

enum Game_Phase 
{
    GAME_PHASE_PLAY,
    GAME_PHASE_LINE
};

struct Piece_State 
{
    
    u8 tetrino_index;
    s32 offset_row;
    s32 offset_col;
    s32 rotation;
};

struct Game_state 
{

    u8 board[WIDTH * HEIGHT];
    u8 lines[HEIGHT];
    s32 pending_line_count;

    Piece_State piece;
    Game_Phase phase;

    s32 start_level;
    s32 level;
    s32 line_count;
    s32 points;

    f32 next_drop_time;
    f32 time;
    f32 highlight_end_time;
};

struct Input_State 
{

    u8 left;
    u8 right;
    u8 up;
    u8 down;
    u8 space;

    s8 dleft;
    s8 dright;
    s8 dup;
    s8 ddown;
    s8 dspace;
};


u8 matrix_get(const u8 *values, s32 width, s32 row, s32 col) 
{
    s32 index = row * width + col;
    return values[index];
}

void matrix_set(u8 *values, s32 width, s32 row, s32 col, u8 value) 
{
    s32 index = row * width + col;
    values[index] = value;
}

u8 tetrino_get(const Tetrino *tetrino, s32 row, s32 col, s32 rotation) 
{

    s32 side = tetrino->side;
    switch (rotation) 
    {
        case 0:
            return tetrino->data[row * side + col];
        case 1:
            return tetrino->data[(side - col - 1) * side + row];
        case 2:
            return tetrino->data[(side - row - 1) * side + (side - col -1)];
        case 3:
            return tetrino->data[col * side + (side - row - 1)];
    }
    return 0;
}

u8 check_row_filled(const u8 *values, s32 width, s32 row)
{
    for (s32 col = 0; col < width; ++col)
    {

        if (!matrix_get(values, width, row, col))
        {
            return 0;
        }

    }
    return 1;
}

s32 find_lines(const u8 *values, s32 width, s32 height, u8 *lines_out)
{
    s32 count = 0;
    for (s32 row = 0; row < height; ++row)
    {
        u8 filled = check_row_filled(values, width, row);
        lines_out[row] = filled;
        count += filled;
    } 
    return count;
}

void clear_lines(u8 *values, s32 width, s32 height, const u8 *lines)
{

    s32 src_row = height - 1;
    for (s32 dst_row = height - 1; dst_row >= 0; --dst_row)
    {
        while (src_row >= 0 && lines[src_row])
        {
            --src_row;
        }

        if (src_row < 0)
        {
            memset(values + dst_row * width, 0, width);
        }
        else
        {
            if (src_row != dst_row)
            {
                memcpy(values + dst_row * width, values + src_row * width, width);
            }
            --src_row;
        }
    }

}

bool check_piece_valid(const Piece_State *piece, const u8 *board, s32 width, s32 height) 
{
 
    const Tetrino *tetrino = TETRINOS + piece->tetrino_index;
    assert(tetrino);

    for (s32 row = 0; row < tetrino->side; ++row) 
    {
        for (s32 col = 0; col < tetrino->side; ++col) 
        {
             u8 value = tetrino_get(tetrino, row, col, piece->rotation);
             if (value > 0) 
             {
                s32 board_row = piece->offset_row + row;
                s32 board_col = piece->offset_col + col;
                if (board_row < 0) 
                {
                   return false;
                }
                if (board_row >= height) 
                {
                     return false;
                }
                if (board_col < 0) 
                {
                    return false;
                }
                if (board_col >= width) 
                {
                    return false;
                }
                if (matrix_get(board, width, board_row, board_col)) 
                {
                    return false;
                }

             }
        }
    }
    return true;
}

void merge_piece(Game_state *game)
{
    const Tetrino *tetrino = TETRINOS + game->piece.tetrino_index;
    for (s32 row = 0; row < tetrino->side; ++row)
    {
        for (s32 col = 0; col < tetrino->side; ++col)
        {
            u8 value = tetrino_get(tetrino, row, col, game->piece.rotation);
            if (value)
            {
                s32 board_row = game->piece.offset_row + row;
                s32 board_col = game->piece.offset_col + col;
                matrix_set(game->board, WIDTH, board_row, board_col, value);
            }
        }
    }
}


u8 random_int(u8 min, u8 max)
{
    u8 range = max - min;
    return min + rand() % range;

}

void spawn_piece(Game_state *game)
{
    game->piece = {};
    game->piece.tetrino_index = random_int(0, ARRAY_COUNT(TETRINOS));
    game->piece.offset_col = WIDTH / 2;
}


f32 get_time_to_next_drop(s32 level)
{
    if(level > 29)
    {
        level = 29;
    }
    return FRAMES_PER_DROP[level] * TARGET_SECONDS_PER_FRAME;
}

bool soft_drop(Game_state *game)
{
    ++game->piece.offset_row;
    if (!check_piece_valid(&game->piece, game->board, WIDTH, HEIGHT))
    {
        --game->piece.offset_row;
        merge_piece(game);
        spawn_piece(game);
        return false;
    }

    game->next_drop_time = game->time + get_time_to_next_drop(game->level);
    return true;
}


s32 compute_points(s32 level, s32 line_count)
{
    switch(line_count)
    {
        case 1:
            return 40 * (level + 1);
        case 2:
            return 100 * (level + 1);
        case 3:
            return 300 * (level + 1);
        case 4:
            return 1200 * (level + 1);
    }
    return 0;
}

s32 min(s32 x, s32 y)
{
    return x < y ? x : y;
}

s32 max(s32 x, s32 y)
{
    return x > y ? x : y;
}

s32 get_lines_for_next_level(s32 start_level, s32 level)
{
    s32 first_level_up_limit = min(start_level * 10 + 10, max(100, start_level * 10 - 50));
    if (level == start_level)
    {
        return first_level_up_limit;
    }
    s32 diff = level - start_level;
    return first_level_up_limit + diff * 10;
}


void update_game_line(Game_state *game)
{
    if (game->time >= game->highlight_end_time)
    {
        clear_lines(game->board, WIDTH, HEIGHT, game->lines);
        game->line_count += game->pending_line_count;
        game->points  += compute_points(game->level, game->pending_line_count);
        
        s32 lines_for_next_level = get_lines_for_next_level(game->start_level, game->level);
        if (game->line_count >= lines_for_next_level)
        {
            ++game->level;
        }        
        game->phase = GAME_PHASE_PLAY;
    }
    
}


void update_game_play(Game_state *game, const Input_State *input) 
{

    Piece_State piece = game->piece;
    if (input->dleft > 0) 
    {
        --piece.offset_col;
    }
    if (input->dright > 0) 
    {
        ++piece.offset_col;
    }
    if (input->dup > 0) 
    {
        piece.rotation = (piece.rotation + 1) % 4;
    }

    if (check_piece_valid(&piece, game->board, WIDTH, HEIGHT)) 
    {
        game->piece = piece;
    }

    if (input->ddown > 0)
    {
        soft_drop(game);
    }

    if (input->dspace > 0)
    {
        while(soft_drop(game));
    }

    while (game->time >= game->next_drop_time)
    {
        soft_drop(game);
    }
    
    game->pending_line_count = find_lines(game->board, WIDTH, HEIGHT, game->lines);
    if (game->pending_line_count > 0)
    {
        game->phase = GAME_PHASE_LINE;
        game->highlight_end_time = game->time + HIGHLIGHT_TIME;
    }
    
}

void update_game(Game_state *game, const Input_State *input) {

    switch(game->phase) 
    {
        case GAME_PHASE_PLAY:
            update_game_play(game, input);
            break;
        case GAME_PHASE_LINE:
            update_game_line(game);
            break;
    }
}


void fill_rect(SDL_Renderer *renderer,
                s32 x, s32 y, s32 width, s32 height, Color color)
{

    SDL_Rect rect = {};
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

void draw_cell(SDL_Renderer *renderer, 
                s32 row, s32 col, u8 value,
                s32 offset_x, s32 offset_y) 
{

    Color base_color = BASE_COLORS[value];
    Color light_color = LIGHT_COLORS[value];
    Color dark_color = DARK_COLORS[value];

    s32 edge = GRID_SIZE / 8;
    s32 x = col * GRID_SIZE + offset_x;
    s32 y = row * GRID_SIZE + offset_y;

    fill_rect(renderer, x, y, GRID_SIZE, GRID_SIZE, dark_color);
    fill_rect(renderer, x + edge, y, 
              GRID_SIZE - edge, GRID_SIZE - edge, light_color);
    fill_rect(renderer, x + edge, y + edge, 
              GRID_SIZE - edge * 2, GRID_SIZE - edge * 2, base_color);


}

void draw_piece(SDL_Renderer *renderer, const Piece_State *piece, 
                s32 offset_x, s32 offset_y) 
{

    const Tetrino *tetrino = TETRINOS + piece->tetrino_index;
    for (s32 row = 0; row < tetrino->side; ++row) 
    {
        for (s32 col = 0; col < tetrino->side; ++col) 
        {
            u8 value = tetrino_get(tetrino, row, col, piece->rotation);
            if(value) 
            {
                draw_cell(renderer, 
                        row + piece->offset_row, 
                        col + piece->offset_col,
                        value, offset_x, offset_y);
                
            }
        }
    }

}

void draw_board(SDL_Renderer *renderer, const u8 *board, s32 width, s32 height, s32 offset_x, s32 offset_y) 
{
    for (s32 row = 0; row < height; ++row)
    {
        for (s32 col = 0; col < width; ++col)
        {
            u8 value = matrix_get(board, width, row, col);
            draw_cell(renderer, row, col, value, offset_x, offset_y);
            // if (value)
            // {
            //     draw_cell(renderer, row, col, value, offset_x, offset_y);
            // }
        }
    }
}


void render_game(const Game_state *game, SDL_Renderer *renderer) 
{
    Color highlight_color = color(0xFF, 0xFF, 0xFF, 0xFF);
    draw_board(renderer, game->board, WIDTH, HEIGHT, 0, 0);
    draw_piece(renderer, &game->piece, 0, 0);

    if (game->phase == GAME_PHASE_LINE)
    {
        for (s32 row = 0; row < HEIGHT; ++row)
        {
            if (game->lines[row])
            {
                s32 x = 0;
                s32 y = row * GRID_SIZE;
                fill_rect(renderer, x, y, WIDTH * GRID_SIZE, GRID_SIZE, highlight_color);
            }
        }
    }


}


int main() 
{
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Tetris", 
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, 
        400, 
        720, 
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    
    Game_state game = {};
    Input_State input = {};

    game.level = 0;
    game.start_level = 0;
    game.phase = GAME_PHASE_PLAY;
    game.next_drop_time = get_time_to_next_drop(game.level);
    spawn_piece(&game);

    bool quit = false;
    while (!quit) 
    {
        game.time = SDL_GetTicks() / 1000.0f;

        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) 
        {
            if(e.type == SDL_QUIT) 
            {
                quit = true;
            }
        }

        s32 key_count;
        const u8 *key_states = SDL_GetKeyboardState(&key_count);


        if (key_states[SDL_SCANCODE_ESCAPE])
        {
            quit = true;
        }

        Input_State prev_input = input;

        input.left = key_states[SDL_SCANCODE_LEFT];
        input.right = key_states[SDL_SCANCODE_RIGHT];
        input.up = key_states[SDL_SCANCODE_UP];
        input.down = key_states[SDL_SCANCODE_DOWN];
        input.space = key_states[SDL_SCANCODE_SPACE];

        input.dleft = input.left - prev_input.left;
        input.dright = input.right - prev_input.right;
        input.dup = input.up - prev_input.up;
        input.ddown = input.down - prev_input.down;
        input.dspace = input.space - prev_input.space;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        update_game(&game, &input);
        render_game(&game, renderer);

        SDL_RenderPresent(renderer);

    }
    
    SDL_DestroyRenderer(renderer);
    SDL_Quit();

    return 0;
}