#include <ncurses.h>
#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <functional>
#define GREEN 1
#define RED 2
#define YELLOW 3
#define MAGENTA 4
#define CYAN 5
#define UP 1
#define DOWN 2
#define RIGHT 3
#define LEFT 4

using namespace std;

class Button {
public: 
    int x, y, width, height;
    bool selected;
    int id;
    string text;
    Button* next;
    Button* prev;
    std::function<void()> action;;
    Button(int X, int Y, int Width, int Height, const string& Text, int ID,
           std::function<void()>Action) 
        : x(X), y(Y), width(Width), height(Height), selected(false), text(Text),
          next(nullptr), prev(nullptr), id(ID), action(Action) {}

    void Select() {
        selected = true;
        for (char& c: text) {
            c = toupper(c);
        }
    }
    void Deselect() {
        selected = false;
        for (char& c: text) {
            c = tolower(c);
        }
    }
    void Exec() {
        action();
    }
};

class Word {
public:
    int x, y;
    string text;
    bool selected;
    std::chrono::high_resolution_clock::time_point lastMoveTime;
    int moveInterval;
    int killingIndex;
    Word(int X, int Y, string Text,int Interval) : x(X), y(Y), text(Text), 
      selected(false), lastMoveTime(std::chrono::high_resolution_clock::now()),
      moveInterval(Interval), killingIndex(0) {}

    /* check to see if the word should be moved */
    void updatePositionIfTime() {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>
                                                  (currentTime - lastMoveTime);
        if (elapsed.count() >= moveInterval) {
            x--;
            lastMoveTime = currentTime;
        }
    }
};
class Barricade {
public: 
    int cost, type, x, y;
    char logo;
    Barricade(int X, int Y, char Logo, int Type): x(X), y(Y), logo(Logo), 
               type(Type) {}
};
class Cursor {
public:
    int x, y, max_x, max_y, min_x, min_y;
    int cost;
    bool enabled;
    char icon;
    int type;
    Cursor(int X, int Y, int Max_X, int Max_Y, int Min_X, int Min_Y, char Icon,
           int Cost, int Type) :        x(X), y(Y), max_x(Max_X), max_y(Max_Y), 
                         min_x(Min_X), min_y(Min_Y), icon(Icon), enabled(true), 
                         cost(Cost), type(Type) {}
    void ChangeIcon(char newIcon) {
        icon = newIcon;
    }
    void Enable() {
        enabled = true;
    }
    void Disable() {
        enabled = false;
    }
    void Up() {
        if (y > min_y) {
            y--;
        }
    }
    void Down() {
        if (y < max_y) {
            y++;
        }
    }
    void Left() {
        if (x > min_x) {
            x--;
        }
    }
    void Right() {
        if (x < max_x) {
            x++;
        }
    }
};
class Window {
public:
    WINDOW * scr;
    int x, y, width, height;
    bool visible;
    Button* head;
    Button* tail;
    Button* selected;
    Cursor* cursor;
    std::vector<Barricade> barricades;
    std::vector<Word> words;
    int income;
    int health;
    bool isKilling;
    int killingIndex;
    int killing = -1;
    Window(int X, int Y, int Width, int Height) 
        : x(X), y(Y), width(Width), height(Height), visible(true), 
          head(nullptr), tail(nullptr), selected(nullptr), cursor(nullptr),
          isKilling(false)
    {
        scr = newwin(height, width, y, x);
    }
    ~Window() {
        Button* current = head;
        Button* next;
        while (current) {
            next = current->next;
            delete current;
            if(next) {
                current = next; 
            }
            /* need this if will run one more time double free ouchy :(*/
            else {
                break;
            }
        }
        delwin(scr);
    }
    void CursorSwitch(int type) {
        // portal
        if(type == 1) {
            cursor->cost = 500;
            cursor->icon = '@';
            cursor->type = 1;

        }
        // spike trap
        if(type == 2) {
            cursor->cost = 1000;
            cursor->icon = 'x';
            cursor->type = 2;
        }
    }
    void DrawVertical(int start_x, int start_y, int direction, int length, char icon) {
        for(int i = 0; i < length; i++) {
            mvwaddch(scr, start_y, start_x, icon);
            if(direction == DOWN) {
                start_y++;
            }
            if(direction == UP) {
                start_y++;
            }
        }
        refresh();
    }
    void DrawCursor() {
        if(income >= cursor->cost) {
            wattron(scr, COLOR_PAIR(GREEN));
        }
        if(income < cursor->cost) {
            wattron(scr, COLOR_PAIR(RED));
        }
        for(const auto& barricade : barricades) {
            if(barricade.y == cursor->y && barricade.x == cursor->x) {
                wattron(scr, COLOR_PAIR(YELLOW));
            }
        }
        mvwaddch(scr, cursor->y, cursor->x, cursor->icon);
        refresh();
        wattroff(scr ,COLOR_PAIR(GREEN));
        wattroff(scr, COLOR_PAIR(RED));
        wattroff(scr, COLOR_PAIR(YELLOW));
    }
    void KillCursor() {
        for(const auto& barricade : barricades) {
            if(barricade.y == cursor->y && barricade.x == cursor->x) {
                mvwaddch(scr, cursor->y, cursor->x, cursor->icon);
                return;
            }
        }
        mvwaddch(scr, cursor->y, cursor->x, ' ');
        refresh();
    }
    void CursorMove(int direction) {
        /* remove the old spot of the cursor */
        mvwaddch(scr, cursor->y, cursor->x, ' ');
        /* remember to replace barricades that cursor covered */
        for(const auto& barricade : barricades) {
            if(barricade.y == cursor->y && barricade.x == cursor->x) {
                mvwaddch(scr, barricade.y, barricade.x, barricade.logo);
            }
        }
        if(direction == UP) {
            cursor->Up();
        }
        if(direction == DOWN) {
            cursor->Down();
        }
        if(direction == LEFT) {
            cursor->Left();
        }
        if(direction == RIGHT) {
            cursor->Right();
        }
    }
    void CursorSelect() {
        for(const auto& barricade : barricades) {
            if(barricade.y == cursor->y && barricade.x == cursor->x) {
                return;
            }
        }
        income -= cursor->cost;
        barricades.emplace_back(cursor->x, cursor->y, cursor->icon, cursor->type);
    }
    void SpawnWord(int y, string text) {
       std::random_device rd2;  
        std::mt19937 gen2(rd2());
        std::uniform_int_distribution<> distr2(50, 200);
        words.emplace_back(154, y, text, distr2(gen2));
    }
    void UpdateWords() {
        for (int i = 0; i < words.size(); i ++) {
            std::string spaces(words[i].text.size(), ' ');
            // leave this one for last
            if(!(i == killing && isKilling) ) {
                mvwaddnstr(scr, words[i].y, words[i].x + 1, spaces.c_str(), 155-words[i].x );
                mvwaddnstr(scr, words[i].y, words[i].x, words[i].text.c_str(), 155 -words[i].x );
            }
            if(killing != -1){
                mvwprintw(scr, words[killing].y, words[killing].x + 1, spaces.c_str());
                // have to print char by char for this one
                wattron(scr, COLOR_PAIR(RED));
                for(int j = 0; j < words[killing].text.size(); j++) {
                    if(j == killingIndex) {
                        wattroff(scr, COLOR_PAIR(RED));
                    }
                    mvwaddch(scr, words[killing].y, words[killing].x + j, words[killing].text[j]);
                } 
            }
        }
    }
    void CheckWordsBarricades() {
        for(int i = 0; i < barricades.size(); i++) {
            // check if every word is hitting :rolling_eyes:
            for(int j = 0; j < words.size(); j++) {
                if(barricades[i].y == words[j].y && barricades[i].x == words[j].x) {
                    // portal
                    std::string spaces(words[j].text.size(), ' ');
                    mvwaddnstr(scr, words[j].y, words[j].x + 1, spaces.c_str(), 154-words[j].x );
                    if(barricades[i].type == 1) {
                        words[j].x += 30;
                        // kill portal trap
                        mvwaddch(scr, barricades[i].y, barricades[i].x, ' ');
                        barricades.erase(barricades.begin() + i);
                    }
                    // spike trap
                    if(barricades[i].type == 2) {
                        // add logic to figure out if this is the selected word
                        if(j == killing && isKilling) {
                            isKilling = false;
                            killingIndex = 0;
                            killing = -1;
                        }
                        mvwaddch(scr, barricades[i].y, barricades[i].x, ' ');
                        words.erase(words.begin() + j);
                        barricades.erase(barricades.begin() + i);
                    }
                    return;
                }
            }
        }

    }
    void DrawHealth() {
        /* if health lost must remove old symbol for it */
        DrawVertical(3,1, DOWN, 48, ' ');
        DrawVertical(4,1, DOWN, 48, ' ');
        DrawVertical(5,1, DOWN, 48, ' ');
        DrawVertical(6,1, DOWN, 48, ' ');

        int subtract = (100 - health) /2;
        wattron(scr, COLOR_PAIR(RED));
        for(int i = 0; i < 48 - subtract; i++) {
            if(i == 10) {
                wattroff(scr, COLOR_PAIR(RED));
                wattron(scr, COLOR_PAIR(MAGENTA));
            }
            if(i == 20) {
                wattroff(scr, COLOR_PAIR(MAGENTA));
                wattron(scr, COLOR_PAIR(YELLOW));
            }
            if(i == 30) {
                wattroff(scr, COLOR_PAIR(YELLOW));
                wattron(scr, COLOR_PAIR(GREEN));
            }
            if(i == 40) {
                wattroff(scr, COLOR_PAIR(GREEN));
                wattron(scr, COLOR_PAIR(CYAN));
            }
            /* do this cause the flash from it laggy kinda looks cool */
            for(int j = 0; j < 4; j++) {
                mvwaddch(scr, 48 - i, 3 + j, ACS_CKBOARD);
            }
        }
        wattroff(scr, COLOR_PAIR(CYAN));
    }
    void KillWords() {
        for(int i = 0; i < words.size(); i++) {
            if(words[i].x < 10) {
                if(i == killing) {
                    killing = -1;
                    isKilling = false;
                    killingIndex = 0;
                }
                else {
                    if(killing > i) {
                        killing--;
                    }
                }
                std::string spaces(words[i].text.size(), ' ');
                mvwprintw(scr, words[i].y, words[i].x + 1, spaces.c_str());
                words.erase(words.begin() + i);
                health--;
            } 
        }
    }
    void DrawBarricades() {
        for(const auto& barricade : barricades) {
            mvwaddch(scr, barricade.y, barricade.x, barricade.logo);
        }
        refresh();
    }
    void HandlePress(int pressed) {
        if(isKilling) {
            // if the key pressed is the key at the index of word being killed
            if(pressed == words[killing].text[killingIndex]) {
                killingIndex++;
                if(killingIndex == words[killing].text.size()) {
                    std::string spaces(words[killing].text.size(), ' ');
                    mvwprintw(scr, words[killing].y, words[killing].x, spaces.c_str());
                    words.erase(words.begin() + killing);
                    // word successfully killed
                    isKilling = false;
                    killing = -1;
                    income += 50;
                }
            }
        }
        else {
            for(int i = words.size() - 1; i >= 0; i--) {
                if(pressed == words[i].text[0]) {
                    killing = i;
                    isKilling = true;
                    killingIndex = 1;
                }
            }
        }
    }
    void AddButton(int x, int y, int width, int height, string text, int id, 
                   std::function<void()>action) {
        if(!head) {
            head = new Button(x, y, width, height, text, id, action);
            tail = head;
            selected = head;
         } else {
             Button* temp = new Button(x, y, width, height, text, id, action);
             tail->next = temp;
             temp->prev = tail;
             tail = temp;
             selected = temp;
         }
         drawButton(selected);
    }
    void SelectButton(int id) {
        Button* node = head;
        while(1) {
            if (node->id == id) {
                selected = node;
                selected->Select();
                drawButton(selected);
                return;
            }
            else {
                if(!node->next) {
                    return;
                }
                node = node->next;
            }
        }
    }
    void SelectNextButton() {
        if(!selected->next) {
            selected = head;
        }
        else {
            selected = selected->next;
        }
        if(selected == head) {
            tail->Deselect();
            drawButton(tail);
        }
        else {
            selected->prev->Deselect();
            drawButton(selected->prev);
        }
        selected->Select();
        drawButton(selected);
    }

    void SelectPrevButton() {
        if(!selected->prev) {
            selected = tail;
        }
        else {
            selected = selected->prev;
        }
        if(selected == tail) {
            head->Deselect();
            drawButton(head);
        }
        else {
            selected->next->Deselect();
            drawButton(selected->next);
        }
        selected->Select();
        drawButton(selected);
    }
    void drawButton(Button* button) {
        if(button->selected) wattron(scr, COLOR_PAIR(GREEN));
        mvwprintw(scr, button->y, button->x,button->text.c_str());
        if(button->selected) wattroff(scr, COLOR_PAIR(GREEN));
    }
    void refresh() {
        wrefresh(scr);
    }
    void clear() {
        wclear(scr);
    }
    void write(string text, int x, int y) {
        mvwprintw(scr, y, x, text.c_str());
    }
    void drawBox() {
        box(scr, 0, 0);
    }
    int grabChar() {
        int pressed = wgetch(scr);
        return pressed;
    }
    
};
void play(Window* prev_win);
void help(Window* prev_win);
void start(Window* prev_win);
void finish(Window* prev_win);

void finish(Window* prev_win) {
    delete prev_win;
    endwin();
    exit(0);
}
void death_screen(Window* prev_win, int round) {
    if(prev_win) {
        delete prev_win;
    }
    Window* end = new Window(1, 1, 170, 60);

    end->write("thanks for playing my game", (170/2) -8, 30);
    std::string round_str = std::to_string(round);
    end->write("you made it to round ", (170/2) -8, 32);
    end->write(round_str, (170/2) + 13, 32);
    end->write("(backspace to exit)", (170/2)- 8, 34);
    end->write("rezenee", (170/2) - 8, 36);

    for(;;){
        int pressed = end->grabChar();
        if(pressed == KEY_BACKSPACE || pressed == '\b') {
            start(end);
        }
    }
}
void updateStats(Window* win, int income, int health, int round_num) {
    // before writing its a good idea to clear it 
    win->write("          ", 10, 2);
    win->write("          ", 10, 4);
    win->write("          ", 10, 6);
    // actually write the values
    win->write(std::to_string(income), 10, 2);
    win->write(std::to_string(health), 10, 4);
    win->write(std::to_string(round_num), 10, 6);
    win->refresh();
}
void round(Window* play, Window* info, int num) {
    // num is the current round number that you are on
    // so this is like active game
    // will have to incrementally spawn words
    // if you start to type a word will begin to be killed
    // kill the cursor
    play->KillCursor();
    int words_to_spawn = 5 + (num * 2);
    int ms = 800;
    ms -= num * 20;
    if(ms < 250) {
        ms = 250;
    }
    // how often the words will spawn
    const auto wordSpawnInterval = std::chrono::milliseconds(ms);
    // start time
    auto lastWordSpawnTime = std::chrono::high_resolution_clock::now();
    std::random_device rd;
    std::mt19937 gen(rd()); 
    std::uniform_int_distribution<> distrib(1, 48);
    nodelay(play->scr, TRUE);
    // read the words
    std::ifstream file("words.txt");
    std::vector<std::string> lines;
    std::random_device rd1;
    std::mt19937 gen1(rd1()); 
    play->DrawVertical(0,0, DOWN, 50, ' ');

    if(file.is_open()) {
        std::string line;
        while(getline(file, line)) {
            lines.push_back(line);

        }
    }
    file.close();
    
    std::uniform_int_distribution<> distrib1(0, lines.size() - 1);

    int old_health = play->health;
    bool killing = false;
    for(;;) {
        if(play->health <= 0) {
            death_screen(play, num);
        }
        if(words_to_spawn <= 0 && play->words.size() <= 0) {
            break;
        }
        int pressed = play->grabChar();
        if(pressed == KEY_BACKSPACE || pressed == '\b') {
            start(play);
        }
        if(pressed != ERR) {
            play->HandlePress(pressed);
        }
        // current time
        auto currentTime = std::chrono::high_resolution_clock::now();
        // current time - start time
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastWordSpawnTime);
        // 
        if (elapsed >= wordSpawnInterval) {
            int random_number = distrib(gen);  
            if(words_to_spawn > 0) {
                play->SpawnWord(random_number, lines[distrib1(gen1)]);
                words_to_spawn--;
            }
            // reset the timer
            lastWordSpawnTime = currentTime;
        }

        for (auto& word : play->words) {
            word.updatePositionIfTime();  
        }
        play->CheckWordsBarricades();
        play->KillWords();
        play->UpdateWords();
        if(play->health != old_health) {
            old_health = play->health;
            play->DrawHealth();
        }
        updateStats(info, play->income, play->health, num);
        play->refresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
void play(Window* prev_win) {
    if(prev_win) {
        wborder(prev_win->scr, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
        prev_win->clear();
        prev_win->refresh();
        delete prev_win;
    }
    Window *play = new Window(1, 1, 155, 50);
    play->drawBox();
    keypad(play->scr, TRUE);
 //   play->refresh();
    Window *info = new Window(1, 51, 170, 10);
    info->drawBox();
    keypad(info->scr, TRUE);
    info->drawBox();
    info->write("income: ", 2, 2);
    info->write("health: ", 2, 4);
    info->write("wave: ", 2, 6);
    info->DrawVertical(143,1, DOWN, 8, '|');
    info->DrawVertical(20,1, DOWN, 8, '|');
    info->write("portal '1': FIVE HUNDRED", 22, 2);
    info->write("spike trap '2': ONE THOUSAND", 22, 4);
    info->write("(space to start round)", 145, 2);
    info->write("(enter to buy)", 145, 4);
    info->write("(backspace to exit)", 145, 6);
    info->refresh();

    Window *decor = new Window(156, 1, 15, 50);
    decor->write("\\_____________", 0, 0);
    decor->write("O-------------\\", 0, 1);
    for(int i = 2; i < 48; i++) {
        decor->write("O-------------|", 0, i);
    }
    decor->write("O-------------|", 0, 48);
    decor->write("\\-------------|", 0, 49);
    decor->refresh();
    play->income = 1000;
    play->health = 100;
    int round_num = 1;

    play->cursor = new Cursor((150/2), (50/2), 153, 48, 10, 1, '@', 500, 1);
    // setting the position x y and choice, also max and min cords

    play->DrawVertical(9, 1, DOWN, 48, ']');
    play->DrawHealth();
    play->DrawCursor();
    updateStats(info, play->income, play-> health, round_num);
    int old_health = play->health;
    for(;;) {
        int pressed = play->grabChar();
        if(pressed == KEY_BACKSPACE || pressed == '\b') {
            start(play);
        }
        if(pressed == '1') {
            play->CursorSwitch(1);
            play->DrawCursor();
        }
        if(pressed == '2') {
            play->CursorSwitch(2);
            play->DrawCursor();
        }
        if(pressed == KEY_UP) {
            play->CursorMove(UP);
            play->DrawCursor();
        }
        if(pressed == KEY_DOWN) {
            play->CursorMove(DOWN);
            play->DrawCursor();
        }
        if(pressed == KEY_LEFT) {
            play->CursorMove(LEFT);
            play->DrawCursor();
        }
        if(pressed == KEY_RIGHT) {
            play->CursorMove(RIGHT);
            play->DrawCursor();
        }
        if(pressed == ' ') {
            round(play, info, round_num);
            // if you return from the function that means that the round is 
            // successfully beat (elsewise you get send to loser screen)
            // so give them the reward, and then they are back in prep phase
            // until they press space again
            play->income += 1000 - (round_num * 10);
            round_num++;
            play->DrawCursor();
            play->drawBox();
            updateStats(info, play->income, play-> health, round_num);
            play->DrawVertical(0,0, DOWN, 50, ' ');
            
            old_health = play->health;
            play->DrawHealth();
        }
        if(pressed == KEY_ENTER || pressed == '\n') {
            if(play->income >= play->cursor->cost) {
                play->CursorSelect();
                play->DrawBarricades();
                play->DrawCursor();
                play->refresh();
                updateStats(info, play->income, play-> health, round_num);
            }
        }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
void drawStart(Window* start) {
    string logo_0 =  "______________                 ___               ___                                                                              ___";
    string logo_1 =  "wwwwwwwwwwwwww__               www               www                                                                              ddd";
    string logo_2 =  "   wwwwwwwwwwwww_                www               www                                                                            ddd";
    string logo_3 =  "    wwwwwwwwwwwww                 www               www                                                                           ddd";
    string logo_4 =  "     wwwwwwwwwwww                 www               www                                                                           ddd";
    string logo_5 =  "     wwwwwwwwwwww__               www               www             ______________________         _______________     ___________ddd"; 
    string logo_6 =  "       wwwwwwwwwwww                _www              _www          oooooooooooooooooooooooo       _rrrrrrrrrrrrrrr    ddddddddddddddd";
    string logo_7 =  "       wwwwwwwwwww__               www               www          ooo                    ooo      rrrrrr/            dddddddd    ddd";
    string logo_8 =  "        wwwwwwwwwwww                _www              _www        ooo                    ooo      rr/                ddddd        ddd";
    string logo_9 =  "        wwwwwwwwwww___              www               www          ooo              .     ooo      rr_              ddd.d         ddd";
    string logo_10 = "          wwwwwwwwwwww                 www               www        ooo            /       ooo      rr             ddddd          ddd";
    string logo_11 = "          wwwwwwwwwwww                 www               www        ooo           /        ooo      rr             ddd.d          ddd";
    string logo_12 = "          wwwwwwwwwwww                 www               www        ooo                    ooo      rr            dddddd          dddd";
    string logo_13 = "          wwwwwwwwwwww                 www               www        ooo                    ooo      rr_           dd.dd            ddd";
    string logo_14 = "          wwwwwwwwwwww                 www               www         ooo       /            ooo     \\rr           ddddddd          ddd";
    string logo_15 = "          wwwwwwwwwwww                 www               www         ooo      /             ooo      rr_          dd.d.ddd         ddd";
    string logo_16 = "          wwwwwwwwwwww                 www               www          ooo    .               ooo     \\rr          ddd.d.ddd____    ddd";
    string logo_17 = "          wwwwwwwwwwww                 www               www          ooo                    ooo      rr          dddddd.ddddddd___ddd       _";
    string logo_18 = "          wwwwwwwwwwww                 www               www          ooo____________________ooo      rr           ddddddddddddddddddd      /|/";
    string logo_19 = "          wwwwwwwwwwww__               www               www           oooooooooooooooooooooooo      rr              dddddddddddddddd     /||/";
    string logo_20 = "            wwwwwwwwwwww                 www               www         __________________________________________________________________//|/";
    string logo_21 = "            wwwwwwwwwwww                 www               www           ___________         ___";  
    string logo_22 = "            wwwwwwwwwwww                _www              _www           wwwwwwwwwww_        www        ___";
    string logo_23 = "            wwwwwwwwwww_                www               www             wwwwwwwwwww_        www       www";
    string logo_24 = "            wwwwwwwwwwww                 www               www              wwwwwwwwwww_      www       www";
    string logo_25 = "            wwwwwwwwwwww                 www               www               wwwwwwwwwww       www       www";
    string logo_26 = "            wwwwwwwwwwww                 www               www               wwwwwwwwwww_       www      www                 _____        _________";
    string logo_27 = "            wwwwwwwwwwww_                www               www                wwwwwwwwwww_       www      www                aaaaa       /rrrrrrrrr\\";
    string logo_28 = "             wwwwwwwwwwww                 www               www                wwwwwwwwwww       www      www          _______ \\aa       rr/     \\rr|";
    string logo_29 = "             wwwwwwwwwwww                 www               www                wwwwwwwwwww       www       www       _/aaaaaaaaaaa       rr|      rrr";
    string logo_30 = "             wwwwwwwwwwww                 www               www                wwwwwwwwwww       wwww       www     /aaaaa      aa       rr|      \\rr";
    string logo_31 = "             wwwwwwwwwwww                 www               www                wwwwwwwwwww        wwww      www     aaa         aa       rr|";
    string logo_32 = "             wwwwwwwwwwww_             ___www            ___www                wwwwwwwwwww        wwww       www    aaa         aa       rr|";
    string logo_33 = "               wwwwwwwwwwww__         _www              _www                   wwwwwwwwwww_      _wwww       www    aaa         aa       rr|";
    string logo_34 = "                 wwwwwwwwwwww       __www              _www                     wwwwwwwwwww_     wwww     ___www    aaa         aa       rr|";
    string logo_35 = "                 wwwwwwwwwwww__   __www          _____www                        wwwwwwwwwww_ __www   ____www       aaa_        aa       rr|";
    string logo_36 = "                   wwwwwwwwwwww___www__     _____wwwwww                           wwwwwwwwwww_wwww____wwwwww        aaaa________aa       rr|         __  ";
    string logo_37 = "                     wwwwwwwwwwwwwwwwww_____wwwwwwwww                              wwwwwwwwwwww  wwwwwww             aaaaaaaaaaaaa       rr/        /||/";
    string logo_38 = "                     wwwwwwwwwwww     wwwwwww                                                                                                      /||/";
    string logo_39 = "                                                                                    ______________________________________________________________//|/";

    start->write(logo_0, 8, 2);
    start->write(logo_1, 8, 3);
    start->write(logo_2, 8, 4);
    start->write(logo_3, 8, 5);
    start->write(logo_4, 8, 6);
    start->write(logo_5, 8, 7);
    start->write(logo_6, 8, 8);
    start->write(logo_7, 8, 9);
    start->write(logo_8, 8, 10);
    start->write(logo_9, 8, 11);
    start->write(logo_10, 8, 12);
    start->write(logo_11, 8, 13);
    start->write(logo_12, 8, 14);
    start->write(logo_13, 8, 15);
    start->write(logo_14, 8, 16);
    start->write(logo_15, 8, 17);
    start->write(logo_16, 8, 18);
    start->write(logo_17, 8, 19);
    start->write(logo_18, 8, 20);
    start->write(logo_19, 8, 21);
    start->write(logo_20, 8, 22);
    start->write(logo_21, 8, 23);
    start->write(logo_22, 8, 24);
    start->write(logo_23, 8, 25);
    start->write(logo_24, 8, 26);
    start->write(logo_25, 8, 27);
    start->write(logo_26, 8, 28);
    start->write(logo_27, 8, 29);
    start->write(logo_28, 8, 30);
    start->write(logo_29, 8, 31);
    start->write(logo_30, 8, 32);
    start->write(logo_31, 8, 33);
    start->write(logo_32, 8, 34);
    start->write(logo_33, 8, 35);
    start->write(logo_34, 8, 36);
    start->write(logo_35, 8, 37);
    start->write(logo_36, 8, 38);
    start->write(logo_37, 8, 39);
    start->write(logo_38, 8, 40);
    start->write(logo_39, 8, 41);
    start->refresh();
}
void drawHelp(Window* help) {
    string help_desc_1 = "the goal of the game is to survive as many waves as possible";
    string help_desc_2 = "to achieve this you must type the enemies to kill them";
    string help_desc_3 = "if the enemies reach your wall they will damage it";
    string help_desc_4 = "after enough damage to your wall game will be over";
    string help_desc_5 = "after each wave you will be awarded income";
    string help_desc_6 = "spend the income on building defenses";
    string help_desc_7 = "defenses";
    string help_desc_8 = "portal '@' : teleports backwards; cost 500. (hotkey 1)";
    string help_desc_9 = "spike trap 'x' : instantly kills word; cost 1000. (hotkey 2)";
    help->write(help_desc_1, 2, 22);
    help->write(help_desc_2, 2, 24);
    help->write(help_desc_3, 2, 26);
    help->write(help_desc_4, 2, 28);
    help->write(help_desc_5, 2, 30);
    help->write(help_desc_6, 2, 32);
    help->write(help_desc_7, 2, 36);
    help->write(help_desc_8, 2, 38);
    help->write(help_desc_9, 2, 40);
    string logo_1  = " ___         ___";
    string logo_2  = "/HHH\\       /HHH\\";
    string logo_3  = "HHHHH       HHHHH                    ____";
    string logo_4  = "HHHHH       HHHHH                    ll\\l";
    string logo_5  = "HHHHH       HHHHH                     l|l";
    string logo_6  = "HHHHH       HHHHH                     l|l";
    string logo_7  = "HHHHH       HHHHH                     l|l";
    string logo_8  = "HHHHH_______HHHHH                     l|l";
    string logo_9  = "HHHHH=======HHHHH        ________     l|l     ___________";
    string logo_10 = "HHHHH/     \\HHHHH       /eeeeeeeee    l|l     ppppppppppp\\";
    string logo_11 = "HHHHH       HHHHH      /eee    eee    l|l     ppp/     pppp";
    string logo_12 = "HHHHH       HHHHH     /eee_____eee    l|l     pp/       ppp";
    string logo_13 = "HHHHH       HHHHH     eee----eee      l|l     pp|        pp";
    string logo_14 = "HHHHH       HHHHH     eeee    eee     l|l     pp|       _pp";
    string logo_15 = "HHHHH       HHHHH     \\eeee    eee    l|l     pp\\      _ppp";
    string logo_16 = "HHHHH       HHHHH      \\eee____eee    l|l     ppp\\____ppppp";
    string logo_17 = "\\HHH/       \\HHH/        \\eeeee/     _l|l_    pppppppppp/";
    string logo_18 = " ---         ---                              pp/";
    string logo_19 = "                                              pp";
    string logo_20 = "                                              pp";
    help->write(logo_1, 2, 1);
    help->write(logo_2, 2, 2);
    help->write(logo_3, 2, 3);
    help->write(logo_4, 2, 4);
    help->write(logo_5, 2, 5);
    help->write(logo_6, 2, 6);
    help->write(logo_7, 2, 7);
    help->write(logo_8, 2, 8);
    help->write(logo_9, 2, 9);
    help->write(logo_10, 2, 10);
    help->write(logo_11, 2, 11);
    help->write(logo_12, 2, 12);
    help->write(logo_13, 2, 13);
    help->write(logo_14, 2, 14);
    help->write(logo_15, 2, 15);
    help->write(logo_16, 2, 16);
    help->write(logo_17, 2, 17);
    help->write(logo_18, 2, 18);
    help->write(logo_19, 2, 19);
    help->write(logo_20, 2, 20);
    help->refresh();
}
void help(Window* prev_win) {
    if(prev_win) {
        delete prev_win;
    }
    Window *help = new Window(1, 1, 170, 60);
    help->drawBox();
    help->AddButton(3,56, 5, 1, "back", 1, [=]() { start(help);});
    help->SelectButton(1);

    /* drawing the ascii for the screen */
    drawHelp(help);
    for(;;) {
        int pressed = help->grabChar();
        if(pressed == KEY_BACKSPACE || pressed == '\b' || pressed == KEY_ENTER || pressed == '\n') {
            help->selected->Exec();
        }
    }
}
void start(Window* prev_win) {
    if(prev_win) {
        delete prev_win;
    }
    Window *start = new Window(1, 1, 170, 60);
    start->drawBox();
    keypad(start->scr, TRUE);
    start->AddButton((170/2)-(4/2), 47, 20, 1, "play", 1, [=]() { play(start);});
    start->AddButton((170/2)-(4/2), 49, 20, 1, "help", 2, [=]() { help(start);});
    start->AddButton((170/2)-(4/2), 51, 20, 1, "quit", 3, [=]() { finish(start);});
    start->SelectButton(1);

    /* drawing the ascii art for the logo */
    drawStart(start);
    for(;;) {
        int pressed = start->grabChar();
        if(pressed == KEY_UP) {
            start->SelectPrevButton();
        }
        if(pressed == KEY_DOWN) {
            start->SelectNextButton();
        }
        if(pressed == KEY_ENTER || pressed == '\n') {
            start->selected->Exec();
        }
        /* even if you aren't selected on the quit button its nice to be able
          to backspace out of the program */
        if(pressed == KEY_BACKSPACE || pressed == '\b') {
            finish(start);
        }
    }
}
int main()
{
    /* BOILER PLATE NCURSES */
	initscr();
	raw();
	noecho();
    start_color();
    curs_set(0);

    /* INIT COLORS */
    init_pair(GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(RED, COLOR_RED, COLOR_BLACK);
    init_pair(YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(CYAN, COLOR_CYAN, COLOR_BLACK);

    if(LINES < 61) {
        endwin();
        printf("game too small, must have atleast lines of 61\n");
        return -1;
    }
    if(COLS < 171) {
        endwin();
        printf("game too small, must have atleast cols of 171\n");
        return -1;
    }
    /* from start rest of game is called */
    start(nullptr);
}
