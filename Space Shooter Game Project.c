// improved_shooter_multibullet.c
// Compile on Windows (MinGW or Visual Studio).
// Example (MinGW): gcc improved_shooter_multibullet.c -o shooter.exe

#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <time.h>
#include <stdlib.h>

#define WIDTH 40
#define HEIGHT 15
#define MAX_ENEMIES 4
#define MAX_LIVES 5
#define MAX_BULLETS 6   // allow up to this many simultaneous bullets

// console
HANDLE hConsole;
void setColor(int color){ SetConsoleTextAttribute(hConsole, color); }
void top(){ COORD coord = {0,0}; SetConsoleCursorPosition(hConsole, coord); }
void clearScreen(){ system("cls"); }

// hide cursor
void hideCursor(){
    CONSOLE_CURSOR_INFO cursorInfo;
    if(GetConsoleCursorInfo(hConsole, &cursorInfo)){
        cursorInfo.bVisible = FALSE;
        cursorInfo.dwSize = 1;
        SetConsoleCursorInfo(hConsole, &cursorInfo);
    }
}

// sounds
void sound_shoot(){ Beep(1200, 45); }
void sound_hit()  { Beep(900, 60); }
void sound_die()  { Beep(400, 220); }

// small utilities
int randRange(int a, int b){ return a + rand() % (b - a + 1); }

// Entities
typedef struct { int x, y; } Bullet;
typedef struct {
    int x, y;
    int hp;
    int maxhp;
    int type; // 0 small,1 med,2 big
    char ch;
} Enemy;

typedef struct {
    int x;
    int y;
    char ch;
    int color;
    int lives;
    int score;
    int level;
} Player;

typedef struct {
    char *name;
    int spawnInterval;
    int enemyHpMultiplier;
} Difficulty;

// Globals
Player player;
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
int tickCounter = 0;
Difficulty difficulties[3] = {
    {"Easy", 12, 1},
    {"Normal", 8, 2},
    {"Hard", 4, 3}
};
int difficultyIndex = 1; // default Normal

// Forward
void spawnEnemy(int idx);
void drawFrame();
void updateLogic();
void homeScreen();
void chooseDifficulty();
void customizePlayer();
void initEnemies();
void resetGame();
void handleCollisionBulletIndex(int bIndex, int prevBulletX);
void updateEnemies();
void drawGUI();
void drawBorders();
void drawHeader();
int spawnBulletAt(int x, int y); // returns bullet index or -1

void initEnemies(){
    for(int i=0;i<MAX_ENEMIES;i++) enemies[i].x = -1;
}

void initBullets(){
    for(int i=0;i<MAX_BULLETS;i++){ bullets[i].x = -1; bullets[i].y = -1; }
}

void spawnEnemy(int idx){
    if(idx < 0 || idx >= MAX_ENEMIES) return;
    Enemy *e = &enemies[idx];
    e->x = 1;
    e->y = randRange(1, WIDTH-2);
    e->type = randRange(0,2);
    int baseHp = (e->type==0)?1 : (e->type==1?2 : 4);
    e->maxhp = baseHp * difficulties[difficultyIndex].enemyHpMultiplier;
    e->hp = e->maxhp;
    e->ch = (e->type==0)?'#' : (e->type==1?'@' : '$');
}

void resetGame(){
    player.x = HEIGHT - 1;           // near bottom
    player.y = WIDTH/2;
    player.lives = MAX_LIVES;
    player.score = 0;
    player.level = 0;
    initEnemies();
    initBullets();
    tickCounter = 0;
}

void drawBorders(){
    for(int i=0;i<WIDTH;i++) printf("-");
    printf("\n");
}

void drawHeader(){
    setColor(11);
    drawBorders();
}

void drawGUI(){
    setColor(7);
    printf("\n Score: %d \t Level: %d \t Difficulty: %s\n", player.score, player.level+1, difficulties[difficultyIndex].name);

    printf(" Lives: ");
    for(int i=0;i<player.lives;i++){
        setColor(12); // red
        printf("\x03"); // heart glyph in some consoles
    }
    if(player.lives == 0) { setColor(7); printf(" (0)"); }
    setColor(7);
    printf("\n\n Controls: 'a' or left arrow = left, 'd' or right arrow = right, space = shoot, q/Esc = quit\n");
}

void drawFrame(){
    top();
    // top border
    drawHeader();

    for(int i=1;i<HEIGHT;i++){
        for(int j=0;j<WIDTH;j++){
            if(j==0 || j==WIDTH-1){
                setColor(11); printf("|");
            } else {
                int drawn = 0;
                // player
                if(i == player.x && j == player.y){
                    setColor(player.color);
                    printf("%c", player.ch);
                    drawn = 1;
                }
                // bullets (draw with priority over enemies for visibility)
                if(!drawn){
                    for(int b=0; b<MAX_BULLETS && !drawn; b++){
                        if(bullets[b].x == i && bullets[b].y == j){
                            setColor(14); printf("|"); drawn = 1;
                        }
                    }
                }
                // enemies
                if(!drawn){
                    for(int e=0;e<MAX_ENEMIES && !drawn;e++){
                        if(enemies[e].x == i && enemies[e].y == j){
                            if(enemies[e].type==0) setColor(10);
                            else if(enemies[e].type==1) setColor(13);
                            else setColor(12);
                            printf("%c", enemies[e].ch);
                            drawn = 1;
                        }
                    }
                }
                if(!drawn) printf(" ");
            }
        }
        setColor(11); printf("|\n");
    }

    // bottom border
    setColor(11); drawBorders();

    // GUI area
    drawGUI();

    // enemy hp summary
    printf("\n Enemies: ");
    for(int e=0;e<MAX_ENEMIES;e++){
        if(enemies[e].x > 0){
            setColor(7); printf("[");
            int hp = enemies[e].hp;
            for(int h=0; h<enemies[e].maxhp; h++){
                if(h < hp) setColor(12), printf("#");
                else setColor(8), printf("-");
            }
            setColor(7); printf("] ");
        }
    }
    printf("\n");
    setColor(7);
}

// Spawn a bullet in first free slot; returns index or -1 if none free
int spawnBulletAt(int x, int y){
    for(int i=0;i<MAX_BULLETS;i++){
        if(bullets[i].x == -1){
            bullets[i].x = x;
            bullets[i].y = y;
            return i;
        }
    }
    return -1;
}

// Collision handling for a single bullet index (robust against crossing)
void handleCollisionBulletIndex(int bIndex, int prevBulletX){
    if(bIndex < 0 || bIndex >= MAX_BULLETS) return;
    if(bullets[bIndex].x == -1 && prevBulletX == -1) return;

    int bX = bullets[bIndex].x;
    int bY = bullets[bIndex].y;

    int minX = bX, maxX = bX;
    if(prevBulletX != -1){
        if(prevBulletX < bX){ minX = prevBulletX; maxX = bX; }
        else { minX = bX; maxX = prevBulletX; }
    }

    for(int e=0;e<MAX_ENEMIES;e++){
        if(enemies[e].x > 0 && enemies[e].y == bY){
            if(enemies[e].x >= minX && enemies[e].x <= maxX){
                // hit
                enemies[e].hp--;
                sound_hit();
                if(enemies[e].hp <= 0){
                    int gained = (enemies[e].type==0)?1 : (enemies[e].type==1?3 : 6);
                    player.score += gained;
                    if(player.score / 5 > player.level) player.level = player.score / 5;
                    enemies[e].x = -1;
                    if(randRange(0,1)) spawnEnemy(e);
                }
                bullets[bIndex].x = -1; // consume bullet
                return;
            }
        }
    }
}

void updateEnemies(){
    for(int e=0;e<MAX_ENEMIES;e++){
        if(enemies[e].x > 0){
            int advInterval = difficulties[difficultyIndex].spawnInterval - (player.level/2);
            if(advInterval < 2) advInterval = 2;
            if(tickCounter % advInterval == 0){
                enemies[e].x++;
            }
            // collision with player
            if(enemies[e].x == player.x && enemies[e].y == player.y){
                player.lives--;
                sound_die();
                enemies[e].x = -1;
                if(player.lives <= 0){
                    // game over handled in game loop
                }
            }
            // enemy passed bottom
            if(enemies[e].x >= HEIGHT){
                player.score -= 1;
                if(player.score < 0) player.score = 0;
                enemies[e].x = -1;
            }
        } else {
            int spawnOdds = 100 - (difficulties[difficultyIndex].spawnInterval * 6);
            spawnOdds -= player.level*2;
            if(spawnOdds < 20) spawnOdds = 20;
            if(randRange(0, spawnOdds) == 0) spawnEnemy(e);
        }
    }
}

void updateLogic(){
    tickCounter++;

    // store previous bullet rows for all bullets
    int prevX[MAX_BULLETS];
    for(int b=0;b<MAX_BULLETS;b++) prevX[b] = bullets[b].x;

    // move bullets up
    for(int b=0;b<MAX_BULLETS;b++){
        if(bullets[b].x != -1){
            bullets[b].x--;
            if(bullets[b].x < 1) bullets[b].x = -1;
        }
    }

    // check bullet collisions using prev & current positions (per bullet)
    for(int b=0;b<MAX_BULLETS;b++){
        handleCollisionBulletIndex(b, prevX[b]);
    }

    // move enemies & handle player collisions
    updateEnemies();

    // check again in case enemy moved into any bullet's new position
    for(int b=0;b<MAX_BULLETS;b++){
        if(bullets[b].x != -1) handleCollisionBulletIndex(b, bullets[b].x);
    }
}

void gameLoop(){
    resetGame();
    clearScreen();

    while(1){
        // input
        if(_kbhit()){
            int c = _getch();
            if(c == 0 || c == 224){
                int c2 = _getch();
                if(c2 == 75 && player.y > 1) player.y--;
                else if(c2 == 77 && player.y < WIDTH-2) player.y++;
            } else {
                if(c == 'a' && player.y > 1) player.y--;
                if(c == 'd' && player.y < WIDTH-2) player.y++;
                if((c == ' ' || c == 72)){
                    // spawn bullet even if other bullets exist (multi-bullet)
                    int bi = spawnBulletAt(player.x - 1, player.y);
                    if(bi != -1) sound_shoot();
                }
                if(c == 'q' || c == 27) break;
            }
        }

        updateLogic();
        drawFrame();

        if(player.lives <= 0){
            top();
            setColor(12);
            printf("\n\n\n\n    *** GAME OVER ***\n    Final Score: %d\n", player.score);
            setColor(7);
            sound_die();
            printf("\n Press any key to return to main menu...");
            _getch();
            break;
        }

        Sleep(60);
    }
}

void chooseDifficulty(){
    int sel = difficultyIndex;
    while(1){
        clearScreen();
        setColor(14);
        printf("Choose Difficulty\n\n");
        for(int i=0;i<3;i++){
            if(i == sel) setColor(10); else setColor(7);
            printf(" %c %s (spawnInterval=%d, enemyHpMult=%d)\n", (i==sel)?'>' : ' ', difficulties[i].name, difficulties[i].spawnInterval, difficulties[i].enemyHpMultiplier);
        }
        setColor(7);
        printf("\n Use 'w'/'s' or up/down to move, Enter to select, q to back.\n");
        int c = _getch();
        if(c == 0 || c == 224){
            int c2 = _getch();
            if(c2 == 72 || c2 == 119) sel = (sel+2)%3;
            if(c2 == 80 || c2 == 115) sel = (sel+1)%3;
        } else {
            if(c == 'w' || c == 'W') sel = (sel+2)%3;
            else if(c == 's' || c == 'S') sel = (sel+1)%3;
            else if(c == '\r'){ difficultyIndex = sel; break; }
            else if(c == 'q' || c == 27) break;
        }
    }
}

void customizePlayer(){
    clearScreen();
    setColor(11);
    printf("Customize Player\n\n");
    setColor(7);
    printf("Choose a character for your player (single key, e.g. ^, A, P). Press key now: ");
    char ch = _getch();
    if(ch != '\r' && ch != '\n') player.ch = ch;

    int colors[] = {7, 10, 11, 12, 13, 14};
    char *names[] = {"White","Green","Cyan","Red","Magenta","Yellow"};
    int sel = 0;
    while(1){
        clearScreen();
        printf("Choose player color\n\n");
        for(int i=0;i<6;i++){
            if(i==sel) setColor(10); else setColor(7);
            SetConsoleTextAttribute(hConsole, colors[i]);
            printf(" %c %s\n", (i==sel)?'>' : ' ', names[i]);
            SetConsoleTextAttribute(hConsole, 7);
        }
        int c = _getch();
        if(c==0||c==224){
            int c2 = _getch();
            if(c2==72) sel = (sel+5)%6;
            if(c2==80) sel = (sel+1)%6;
        } else {
            if(c=='w') sel = (sel+5)%6;
            else if(c=='s') sel = (sel+1)%6;
            else if(c=='\r'){ player.color = colors[sel]; break; }
            else if(c=='q' || c==27) break;
        }
    }
}

void homeScreen(){
    while(1){
        clearScreen();
        setColor(14);
        printf("=====================================\n");
        setColor(11);
        printf("      CONSOLE SPACE SHOOTER 2.0      \n");
        setColor(14);
        printf("=====================================\n\n");
        setColor(7);
        printf(" 1) Start Game\n 2) Difficulty (current: %s)\n 3) Customize Player\n 4) Quit\n\n", difficulties[difficultyIndex].name);
        printf(" Choose option number: ");
        int c = _getch();
        if(c=='1' || c=='\r'){
            gameLoop();
        } else if(c=='2'){
            chooseDifficulty();
        } else if(c=='3'){
            customizePlayer();
        } else if(c=='4' || c=='q' || c==27){
            clearScreen();
            setColor(7);
            printf("Bye!\n");
            return;
        }
    }
}

int main(){
    srand((unsigned int)time(NULL));
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    hideCursor();

    // defaults
    player.ch = '^';
    player.color = 14; // yellow
    difficultyIndex = 1; // Normal
    initEnemies();
    initBullets();

    homeScreen();
    return 0;
}
