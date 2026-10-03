// ============================================================================
//  Домашнее задание 1 — «Змейка на C++» (базовая программа)
//  Среда: VS Code + WSL (Linux). Компиляция:
//      g++ -std=c++17 -O2 -Wall -o snake snake.cpp
//  Запуск:  ./snake
//
//  Управление:
//      W / S / A / D  или стрелки — движение змейки
//      H              — вызвать справку (правила игры) в любой момент
//      Q              — выйти из игры
// ============================================================================

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <unistd.h>   // read(), usleep()
#include <termios.h>  // настройка терминала (безэховый режим)

using namespace std;

// ----------------------------- Константы игры -------------------------------
const int FIELD_SIZE = 10;            // поле 10x10
const int MAX_LEN    = FIELD_SIZE * FIELD_SIZE; // максимум клеток (потолок длины)
const int DELAY_US   = 250000;        // задержка между ходами, микросекунды (0.25 с)

// Координаты клетки поля
struct Point {
    int x; // строка
    int y; // столбец
};

// Направления движения (индексы массива dirX/dirY)
enum Dir { DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3 };

// Векторы смещения для каждого направления
const int dirX[4] = {-1, 1, 0, 0};
const int dirY[4] = { 0, 0,-1, 1};

// --------------------------------- Терминал ---------------------------------
// Включаем режим без эха и посимвольного ввода (чтобы клавиши читались сразу)
void setupTerminal(termios &oldTio) {
    tcgetattr(STDIN_FILENO, &oldTio);          // сохраняем текущие настройки
    termios newTio = oldTio;
    newTio.c_lflag &= ~(ICANON | ECHO);        // выключаем канонический режим и эхо
    newTio.c_cc[VMIN]  = 0;                    // read не блокируется
    newTio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newTio); // применяем новые настройки
}

// Возвращаем исходные настройки терминала (обязательно перед завершением!)
void restoreTerminal(const termios &oldTio) {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTio);
}

// ---------------------------------- Справка ----------------------------------
// Краткое описание цели игры, правил и клавиш управления.
// Вызывается перед началом игры и по клавише H во время игры.
void printHelp() {
    cout << "\n=================== СПРАВКА: ЗМЕЙКА ===================\n"
         << "Цель игры: управлять змейкой, съедать как можно больше еды.\n"
         << "           Каждая съеденная еда увеличивает счёт на 1 и\n"
         << "           делает змейку длиннее на 1 сегмент.\n\n"
         << "Правила:\n"
         << "  - Поле 10x10, змейка стартует из центра поля длиной 1.\n"
         << "  - Еда появляется случайно на свободных клетках.\n"
         << "  - Столкновение со стеной или собственным хвостом = проигрыш.\n"
         << "  - Нельзя развернуться на 180 градусов.\n\n"
         << "Клавиши управления:\n"
         << "  W или стрелка вверх    — движение вверх\n"
         << "  S или стрелка вниз     — движение вниз\n"
         << "  A или стрелка влево    — движение влево\n"
         << "  D или стрелка вправо   — движение вправо\n"
         << "  H                      — показать эту справку\n"
         << "  Q                      — выйти из игры\n"
         << "=======================================================\n";
}

// ------------------------------ Чтение ввода --------------------------------
// Читает нажатую клавишу (неблокирующе). Поддерживает WASD, стрелки, H, Q.
// Возвращает код направления, либо -1 (ничего/ошибка), либо специальные коды:
//   HELP_CODE = 100, EXIT_CODE = 101
const int HELP_CODE = 100;
const int EXIT_CODE = 101;

int readKey() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1; // нет ввода

    if (c == 'w' || c == 'W') return DIR_UP;
    if (c == 's' || c == 'S') return DIR_DOWN;
    if (c == 'a' || c == 'A') return DIR_LEFT;
    if (c == 'd' || c == 'D') return DIR_RIGHT;
    if (c == 'h' || c == 'H') return HELP_CODE;
    if (c == 'q' || c == 'Q') return EXIT_CODE;

    // Экранированные последовательности стрелок: ESC [ A/B/C/D
    if (c == '\x1b') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) == 1 &&
            read(STDIN_FILENO, &seq[1], 1) == 1 && seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return DIR_UP;
                case 'B': return DIR_DOWN;
                case 'C': return DIR_RIGHT;
                case 'D': return DIR_LEFT;
            }
        }
    }
    return -1; // неизвестный символ — игнорируем
}

// ------------------------------- Отрисовка ----------------------------------
// Печатает игровое поле. Массивы тела змейки передаются как указатели
// (использование массивов: body[] — кольцевой буфер, grid[][] — карта поля).
void drawField(const Point *body, int headIdx, int length, const Point &food, int score) {
    char grid[FIELD_SIZE][FIELD_SIZE]; // вспомогательная карта для отрисовки

    // 1. пустое поле
    for (int i = 0; i < FIELD_SIZE; ++i)
        for (int j = 0; j < FIELD_SIZE; ++j)
            grid[i][j] = '.';

    // 2. размечаем еду
    grid[food.x][food.y] = '*';

    // 3. размечаем тело змейки (голова — 'O', хвост — 'o')
    for (int k = 0; k < length; ++k) {
        int idx = (headIdx - k + MAX_LEN * 2) % MAX_LEN; // идём от головы к хвосту
        grid[body[idx].x][body[idx].y] = (k == 0) ? 'O' : 'o';
    }

    // 4. выводим поле с координатной сеткой
    cout << "\n  Юг (S / вниз)\n";
    cout << "    ";
    for (int j = 0; j < FIELD_SIZE; ++j) cout << j;
    cout << "\n";
    for (int i = 0; i < FIELD_SIZE; ++i) {
        cout << setw(2) << i << " ";
        for (int j = 0; j < FIELD_SIZE; ++j) {
            cout << grid[i][j] << ' ';
        }
        if (i == 0) cout << " <- Север (W / вверх)";
        cout << "\n";
    }
    cout << "Запад (A)                          Восток (D)\n";
    cout << "Счёт: " << score
         << "   Длина: " << length
         << "   [H — справка, Q — выход]\n";
}

// --------------------------- Появление новой еды ----------------------------
// Ищем все свободные клетки и выбираем случайную (еда никогда не появляется
// на теле змейки). Используется массив свободных клеток freeCells[].
Point spawnFood(const Point *body, int headIdx, int length) {
    Point freeCells[MAX_LEN];
    int freeCount = 0;

    // Карта занятых клеток
    bool taken[FIELD_SIZE][FIELD_SIZE] = {};
    for (int k = 0; k < length; ++k) {
        int idx = (headIdx - k + MAX_LEN * 2) % MAX_LEN;
        taken[body[idx].x][body[idx].y] = true;
    }

    for (int i = 0; i < FIELD_SIZE; ++i)
        for (int j = 0; j < FIELD_SIZE; ++j)
            if (!taken[i][j])
                freeCells[freeCount++] = Point{i, j};

    // Если свободных клеток нет — поле заполнено (победа недостижима в базе,
    // но защита от выхода за границы массива нужна)
    if (freeCount == 0) return Point{-1, -1};

    return freeCells[rand() % freeCount];
}

// ------------------------------ Основная игра -------------------------------
// Реализует цикл: ввод -> ход -> проверка столкновений -> отрисовка.
// Змейка хранится в массиве-«кольцевом буфере» body[] (голова двигается,
// хвост остаётся, при росте просто не сдвигаем индекс хвоста).
bool runGame() {
    Point body[MAX_LEN];       // массив сегментов тела змейки
    int headIdx = 0;           // индекс головы в кольцевом буфере
    int length = 1;            // текущая длина змейки
    int score  = 0;            // счёт игрока

    // Змейка стартует в центре поля, направление — вправо
    Point start = {FIELD_SIZE / 2, FIELD_SIZE / 2};
    body[0] = start;
    Dir dir = DIR_RIGHT;

    Point food = spawnFood(body, headIdx, length);

    bool gameOver = false;
    bool won = false;

    while (!gameOver) {
        drawField(body, headIdx, length, food, score);

        // --- Опрос ввода до следующего хода ---
        int nextDir = -1;
        bool exitRequested = false;
        while (true) {
            int key = readKey();
            if (key == -1) break;             // новых нажатий нет
            if (key == HELP_CODE) {
                printHelp();                  // справка по клавише H
                continue;
            }
            if (key == EXIT_CODE) {           // выход по клавише Q
                exitRequested = true;
                break;
            }
            nextDir = key;                    // направление движения
        }
        if (exitRequested) {
            cout << "\nИгра прервана по клавише Q. Счёт: " << score << "\n";
            return true;
        }

        // Разворот на 180 градусов запрещён (иначе змейка сразу в себя врежется)
        if (nextDir != -1) {
            bool opposite = (nextDir == DIR_UP   && dir == DIR_DOWN) ||
                            (nextDir == DIR_DOWN && dir == DIR_UP)   ||
                            (nextDir == DIR_LEFT && dir == DIR_RIGHT)||
                            (nextDir == DIR_RIGHT&& dir == DIR_LEFT);
            if (!opposite) dir = static_cast<Dir>(nextDir);
        }

        // --- Шаг змейки: вычисляем новую голову ---
        Point newHead{body[headIdx].x + dirX[dir], body[headIdx].y + dirY[dir]};

        // Проверка столкновения со стеной
        if (newHead.x < 0 || newHead.x >= FIELD_SIZE ||
            newHead.y < 0 || newHead.y >= FIELD_SIZE) {
            gameOver = true;
            cout << "\n*** СТОЛКНОВЕНИЕ СО СТЕНОЙ! Игра окончена. ***\n";
        }

        // Проверка столкновения с собственным хвостом
        // (хвост сдвинется, если еда не съедена — его трогать можно)
        if (!gameOver) {
            bool ate = (newHead.x == food.x && newHead.y == food.y);
            int checkLen = ate ? length : length - 1; // без последней клетки хвоста
            for (int k = 0; k < checkLen; ++k) {
                int idx = (headIdx - k + MAX_LEN * 2) % MAX_LEN;
                if (body[idx].x == newHead.x && body[idx].y == newHead.y) {
                    gameOver = true;
                    cout << "\n*** СТОЛКНОВЕНИЕ С ХВОСТОМ! Игра окончена. ***\n";
                    break;
                }
            }
        }

        if (!gameOver) {
            // Двигаем голову в кольцевом буфере
            headIdx = (headIdx + 1) % MAX_LEN;
            body[headIdx] = newHead;

            if (newHead.x == food.x && newHead.y == food.y) {
                // Еда съедена: длина +1, счёт +1, новая еда
                ++length;
                ++score;
                food = spawnFood(body, headIdx, length);
                if (food.x == -1) { // поле полностью заполнено
                    won = true;
                    gameOver = true;
                    cout << "\n*** ПОБЕДА! Поле заполнено полностью! ***\n";
                }
            } else {
                // Не съели — хвост «освобождается»: длина не растёт.
                // В кольцевом буфере старые клетки просто перезаписываются,
                // поэтому достаточно не увеличивать length.
            }
            usleep(DELAY_US); // темп игры
        }
    }

    // Финальный кадр и итоговый счёт
    drawField(body, headIdx, length, food, score);
    cout << "\nИтоговый счёт: " << score << "\n";
    if (won) cout << "Вы победили!\n";
    return true;
}

// ----------------------------------- Main -----------------------------------
int main() {
    srand(static_cast<unsigned>(time(nullptr))); // сид для rand()

    cout << "Добро пожаловать в игру «Змейка»!\n";
    printHelp(); // обязательная справка перед началом игры

    cout << "\nНажмите любую клавишу (кроме Q) для старта...\n";

    termios oldTio;
    setupTerminal(oldTio);                 // безэховый посимвольный ввод

    // Ждём любое нажатие для старта (Q — сразу выход)
    int key;
    do {
        key = readKey();
        if (key == -1) usleep(50000);
    } while (key == -1);

    bool ok = true;
    if (key == EXIT_CODE) {
        cout << "\nВыход. До встречи!\n";
        ok = false;
    } else {
        ok = runGame();
    }

    restoreTerminal(oldTio);               // обязательно восстанавливаем терминал
    return ok ? 0 : 1;
}
