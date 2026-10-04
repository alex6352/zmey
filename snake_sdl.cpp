// ============================================================================
//  Змейка на C++ + SDL2 — графическая версия (визуализатор вместо терминала)
//  Домашнее задание 1. Тот же набор правил, что и в консольной версии:
//    - поле 10x10, змейка стартует из центра, длина 1;
//    - еда появляется случайно на свободных клетках;
//    - съел еду -> длина +1, счёт +1;
//    - столкновение со стеной или хвостом -> проигрыш + итоговый счёт;
//    - справка вызывается во время игры (клавиша H), перед игрой показаны
//      правила и управление.
//
//  Сборка (WSL / Linux):
//      g++ -std=c++17 -O2 -Wall snake_sdl.cpp -o snake_sdl $(sdl2-config --cflags --libs)
//  Запуск:
//      ./snake_sdl
//  Установка SDL2 в WSL-Ubuntu: sudo apt install libsdl2-dev
// ============================================================================

#include <SDL2/SDL.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <ctime>

// --------------------------------- Константы --------------------------------
const int GRID = 10;                       // размер поля 10x10 (клеток)
const int CELL = 48;                       // размер клетки в пикселях
const int TOPBAR = 56;                     // высота панели со счётом
const int WIN_W = GRID * CELL;             // ширина окна
const int WIN_H = GRID * CELL + TOPBAR;    // высота окна
const int FPS_MS = 150;                    // пауза между ходами (мс)

enum Dir { DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3 };
// dirX — смещение по горизонтали (x), dirY — смещение по вертикали (y).
// Важно: индексы соответствуют enum Dir (UP/LEFT уменьшают координату).
const int dirX[4] = { 0, 0,-1, 1};         // dx для UP, DOWN, LEFT, RIGHT
const int dirY[4] = {-1, 1, 0, 0};         // dy для UP, DOWN, LEFT, RIGHT

// Цвета (RGB)
const SDL_Color COL_BG     = { 24, 26, 34 , 255};
const SDL_Color COL_GRID   = { 40, 44, 56 , 255};
const SDL_Color COL_SNAKE  = { 60, 200, 110, 255};
const SDL_Color COL_HEAD   = { 120, 255, 160, 255};
const SDL_Color COL_FOOD   = { 240, 80, 80 , 255};
const SDL_Color COL_TEXT   = { 230, 230, 235, 255};
const SDL_Color COL_PANEL  = { 16, 18, 24 , 255};

// ------------------------------ Простой текст -------------------------------
// Рендерим текст без внешних шрифтов: встроенный 8x8 bitmap-шрифт SDL_ttf не
// используем (чтобы не ставить доп. библиотеки). Символы описаны масками строк.
// Каждый символ задан 7 строками по 5 бит (старший бит строки = левый пиксель)
struct Glyph { char ch; unsigned char rows[8]; };

static const Glyph FONT[] = {
 {' ',{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
 {'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11,0x00}},
 {'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E,0x00}},
 {'C',{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E,0x00}},
 {'D',{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E,0x00}},
 {'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F,0x00}},
 {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10,0x00}},
 {'G',{0x0E,0x11,0x10,0x17,0x11,0x11,0x0E,0x00}},
 {'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11,0x00}},
 {'I',{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E,0x00}},
 {'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C,0x00}},
 {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11,0x00}},
 {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F,0x00}},
 {'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11,0x00}},
 {'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11,0x00}},
 {'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E,0x00}},
 {'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10,0x00}},
 {'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D,0x00}},
 {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11,0x00}},
 {'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E,0x00}},
 {'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0x00}},
 {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E,0x00}},
 {'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04,0x00}},
 {'W',{0x11,0x11,0x11,0x15,0x15,0x1B,0x11,0x00}},
 {'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11,0x00}},
 {'Y',{0x11,0x11,0x0A,0x04,0x04,0x04,0x04,0x00}},
 {'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F,0x00}},
 {'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E,0x00}},
 {'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E,0x00}},
 {'2',{0x0E,0x11,0x01,0x06,0x08,0x10,0x1F,0x00}},
 {'3',{0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E,0x00}},
 {'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02,0x00}},
 {'5',{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E,0x00}},
 {'6',{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E,0x00}},
 {'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08,0x00}},
 {'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E,0x00}},
 {'9',{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C,0x00}},
 {'-',{0x00,0x00,0x00,0x0E,0x00,0x00,0x00,0x00}},
 {'.',{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}},
 {',',{0x00,0x00,0x00,0x00,0x0C,0x04,0x08,0x00}},
 {':',{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00,0x00}},
 {'!',{0x04,0x04,0x04,0x04,0x04,0x00,0x04,0x00}},
 {'?',{0x0E,0x11,0x01,0x06,0x04,0x00,0x04,0x00}},
 {'/',{0x01,0x01,0x02,0x04,0x08,0x10,0x10,0x00}},
 {'%',{0x11,0x01,0x02,0x04,0x08,0x10,0x11,0x00}},
 {'+',{0x00,0x04,0x04,0x1F,0x04,0x04,0x00,0x00}},
 {'=',{0x00,0x00,0x1F,0x00,0x1F,0x00,0x00,0x00}},
 {'(',{0x02,0x04,0x08,0x08,0x08,0x04,0x02,0x00}},
 {')',{0x08,0x04,0x02,0x02,0x02,0x04,0x08,0x00}}
};

// Функция поиска глифа; отсутствующие символы рисуем как '?'
static const Glyph* findGlyph(char c) {
    if ((unsigned char)c >= 0x80) { static const Glyph sp{' ',{0,0,0,0,0,0,0,0}}; return &sp; }
    for (const auto &g : FONT) if (g.ch == c) return &g;
    static const Glyph q = {'?',{0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}};
    return &q;
}

// Нарисовать строку текста масштабированным bitmap-шрифтом
// Ширина строки в пикселях при данном масштабе
static int textWidth(const char *text, int scale) {
    int n = 0;
    for (const char *p = text; *p; ++p) ++n;
    return n > 0 ? n * 6 * scale - scale : 0; // 5px глиф + 1px интервал
}

static void drawText(SDL_Renderer *r, int x, int y, const char *text,
                     SDL_Color color, int scale = 2) {
    if (x == INT_MIN) x = (WIN_W - textWidth(text, scale)) / 2; // центрирование
    int cx = x;
    for (const char *p = text; *p; ++p) {
        const Glyph *g = findGlyph(*p);
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (g->rows[row] & (0x80 >> col)) {
                    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
                    SDL_Rect px = { cx + col*scale, y + row*scale, scale, scale };
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
        cx += 6 * scale; // ширина глифа 5px + 1px межбуквенный интервал
    }
}

// ---------------------------------- Игра ------------------------------------
struct Cell { int x, y; };

class Game {
public:
    Cell body[GRID*GRID];   // массив сегментов тела (кольцевой буфер)
    int headIdx = 0;        // индекс головы в кольцевом буфере
    int length = 1;         // текущая длина змейки
    Dir dir = DIR_RIGHT;    // текущее направление
    Cell food{};            // позиция еды
    int score = 0;          // счёт
    bool gameOver = false;  // флаг проигрыша
    bool paused = false;    // пауза (клавиша P)
    bool showHelp = false;  // оверлей справки (клавиша H)

    Game() { reset(); }

    void reset() {
        // Змейка стартует в центре поля, длина равна 1
        body[0] = { GRID/2, GRID/2 };
        headIdx = 0; length = 1; dir = DIR_RIGHT;
        score = 0; gameOver = false; paused = false;
        spawnFood();
    }

    // Клетка занята телом змейки?
    bool isBody(int x, int y) const {
        for (int i = 0; i < length; ++i) {
            const Cell &c = body[(headIdx - i + GRID*GRID) % (GRID*GRID)];
            if (c.x == x && c.y == y) return true;
        }
        return false;
    }

    // Появление еды на случайной СВОБОДНОЙ клетке (массив свободных клеток)
    void spawnFood() {
        Cell freeCells[GRID*GRID];
        int n = 0;
        for (int y = 0; y < GRID; ++y)
            for (int x = 0; x < GRID; ++x)
                if (!isBody(x, y)) freeCells[n++] = { x, y };
        if (n == 0) { gameOver = true; return; } // поле заполнено — победа/конец
        food = freeCells[rand() % n];
    }

    // Один шаг игры: движение, проверка столкновений и поедания еды
    void step() {
        if (gameOver || paused) return;
        Cell next = { body[headIdx].x + dirX[dir],
                      body[headIdx].y + dirY[dir] };
        // Столкновение со стеной -> проигрыш
        if (next.x < 0 || next.x >= GRID || next.y < 0 || next.y >= GRID) {
            gameOver = true; return;
        }
        // Хвост в этом шаге сдвигается, поэтому последняя клетка свободна,
        // если еда не съедена
        bool willEat = (next.x == food.x && next.y == food.y);
        for (int i = 0; i < length - (willEat ? 0 : 1); ++i) {
            const Cell &c = body[(headIdx - i + GRID*GRID) % (GRID*GRID)];
            if (c.x == next.x && c.y == next.y) { gameOver = true; return; }
        }
        // Двигаем голову (кольцевой буфер)
        headIdx = (headIdx + 1) % (GRID*GRID);
        body[headIdx] = next;
        if (willEat) { ++length; ++score; spawnFood(); }
    }

    // Смена направления с запретом разворота на 180 градусов
    void turn(Dir d) {
        if ((d == DIR_UP && dir == DIR_DOWN) || (d == DIR_DOWN && dir == DIR_UP) ||
            (d == DIR_LEFT && dir == DIR_RIGHT) || (d == DIR_RIGHT && dir == DIR_LEFT))
            return;
        dir = d;
    }
};

// ------------------------------- Отрисовка ----------------------------------
static void drawGame(SDL_Renderer *r, const Game &g) {
    // Фон
    SDL_SetRenderDrawColor(r, COL_BG.r, COL_BG.g, COL_BG.b, COL_BG.a);
    SDL_RenderClear(r);

    // Верхняя панель: счёт и подсказка
    SDL_SetRenderDrawColor(r, COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, COL_PANEL.a);
    SDL_Rect panel = {0, 0, WIN_W, TOPBAR};
    SDL_RenderFillRect(r, &panel);
    char buf[64];
    snprintf(buf, sizeof(buf), "SCORE: %d", g.score);
    drawText(r, 12, 8, buf, COL_TEXT, 3);
    drawText(r, INT_MIN, 38, "H HELP   P PAUSE   R RESTART   ESC QUIT", COL_TEXT, 1);

    // Сетка поля
    SDL_SetRenderDrawColor(r, COL_GRID.r, COL_GRID.g, COL_GRID.b, COL_GRID.a);
    for (int i = 0; i <= GRID; ++i) {
        SDL_RenderDrawLine(r, i*CELL, TOPBAR, i*CELL, TOPBAR + GRID*CELL);
        SDL_RenderDrawLine(r, 0, TOPBAR + i*CELL, WIN_W, TOPBAR + i*CELL);
    }

    // Еда (квадрат с отступом)
    SDL_SetRenderDrawColor(r, COL_FOOD.r, COL_FOOD.g, COL_FOOD.b, COL_FOOD.a);
    SDL_Rect fd = { g.food.x*CELL+6, TOPBAR + g.food.y*CELL+6, CELL-12, CELL-12 };
    SDL_RenderFillRect(r, &fd);

    // Змейка: голова ярче туловища
    for (int i = 0; i < g.length; ++i) {
        const Cell &c = g.body[(g.headIdx - i + GRID*GRID) % (GRID*GRID)];
        SDL_Color col = (i == 0) ? COL_HEAD : COL_SNAKE;
        SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
        SDL_Rect rc = { c.x*CELL+2, TOPBAR + c.y*CELL+2, CELL-4, CELL-4 };
        SDL_RenderFillRect(r, &rc);
    }

    // Оверлей справки (как клавиша H в консольной версии)
    if (g.showHelp) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
        SDL_Rect box = { 40, 80, WIN_W-80, WIN_H-160 };
        SDL_RenderFillRect(r, &box);
        int y = 90;
        drawText(r, INT_MIN, y, "SNAKE - HELP", COL_TEXT, 2); y += 34;
        drawText(r, INT_MIN, y, "GOAL: EAT FOOD AND GROW.", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "LOSE IF YOU HIT A WALL", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "OR YOUR OWN TAIL.", COL_TEXT, 1); y += 20;
        drawText(r, INT_MIN, y, "CONTROLS:", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "ARROWS / W A S D - MOVE", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "H - THIS HELP", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "P - PAUSE, R - RESTART", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "ESC - QUIT", COL_TEXT, 1);
    }

    // Экран проигрыша с итоговым счётом
    if (g.gameOver) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
        SDL_Rect box = { 0, TOPBAR, WIN_W, GRID*CELL };
        SDL_RenderFillRect(r, &box);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 - 40, "GAME OVER!", COL_FOOD, 3);
        char sc[48]; snprintf(sc, sizeof(sc), "FINAL SCORE: %d", g.score);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 + 8, sc, COL_TEXT, 2);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 + 44, "PRESS R TO RESTART", COL_TEXT, 1);
    }
}

// --------------------------------- main -------------------------------------
int main(int, char*[]) {
    srand((unsigned)time(nullptr));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        fprintf(stderr, "Проверьте, что установлен libsdl2-dev и есть дисплей"
                        " (в WSL2 нужен X/Wayland-сервер, например VcXsrv).\n");
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("Snake C++ (SDL2)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED |
                                                      SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "CreateWindow/Renderer failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    Game game;
    bool running = true;
    Uint32 lastTick = SDL_GetTicks();

    // Главный игровой цикл
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_h: game.showHelp = !game.showHelp; break;
                    case SDLK_p: game.paused = !game.paused; break;
                    case SDLK_r: game.reset(); break;
                    case SDLK_UP: case SDLK_w:    game.turn(DIR_UP);    break;
                    case SDLK_DOWN: case SDLK_s:  game.turn(DIR_DOWN);  break;
                    case SDLK_LEFT: case SDLK_a:  game.turn(DIR_LEFT);  break;
                    case SDLK_RIGHT: case SDLK_d: game.turn(DIR_RIGHT); break;
                    default: break;
                }
            }
        }

        // Шаг змейки раз в FPS_MS миллисекунд
        Uint32 now = SDL_GetTicks();
        if (now - lastTick >= (Uint32)FPS_MS) {
            game.step();
            lastTick = now;
        }

        drawGame(ren, game);
        SDL_RenderPresent(ren);
        SDL_Delay(5); // чтобы не грузить CPU
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
