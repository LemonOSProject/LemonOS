#include <lemon/gui/Window.h>
#include <lemon/gui/Messagebox.h>

#include <map>
#include <functional>

Lemon::GUI::Window* window;
Lemon::GUI::WindowMenuBar menuBar;

enum {
    EasyDifficulty = 0,
    MediumDifficulty = 1,
    HardDifficulty = 2,
};

std::map<int, Vector2i> difficultySizes = {{EasyDifficulty, {10, 10}}, {MediumDifficulty, {16, 16}}, {HardDifficulty, {24, 24}}};
std::map<int, int> difficultyMineCounts = {{EasyDifficulty, 12}, {MediumDifficulty, 48}, {HardDifficulty, 96}};

enum {
    EmptyTile,
    MineTile,
};

struct Tile{
    bool hidden = true;
    bool flagged = false;
    int type = EmptyTile;
    int surroundingMines = 0;
};

surface_t resources;

class MinesweeperGame : public Lemon::GUI::Widget{
    int difficulty = EasyDifficulty;
    Tile** tiles = nullptr;
    vector2i_t mapSize;
    int unflaggedMines = -1;

    void GameOver(){
        for(int i = 0; i < mapSize.y; i++){
            for(int j = 0; j < mapSize.x; j++){
                Tile& tile = tiles[i][j];

                tile.hidden = false;
            }
        }

        window->Paint();
        Lemon::GUI::DisplayMessageBox("Minesweeper", "Game Over!");

        Generate(difficulty);
    }

    void CheckWin(){
        unflaggedMines = 0;
        for(int i = 0; i < mapSize.y; i++){
            for(int j = 0; j < mapSize.x; j++){
                Tile& tile = tiles[i][j];

                if(tile.type == MineTile && !tile.flagged){
                    unflaggedMines++;
                    break;
                }
            }
        }

        if(unflaggedMines){
            return;
        }

        window->Paint();
        Lemon::GUI::DisplayMessageBox("Minesweeper", "You Win!");

        Generate(difficulty);
    }
public:
    MinesweeperGame(int diff) : Widget({0,0,0,0}){
        Generate(diff);
    }

    void Generate(int diff){
        if(tiles){
            for(int i = 0; i < mapSize.y; i++){
                delete tiles[i];
            }

            delete tiles;
        }

        difficulty = diff;

        mapSize = difficultySizes[difficulty];
        int mineCount = difficultyMineCounts[difficulty];

        bounds = {0, 0, mapSize.x * 16, mapSize.y * 16};
        UpdateFixedBounds();

        tiles = new Tile*[mapSize.y];
        for(int i = 0; i < mapSize.y; i++)
            tiles[i] = new Tile[mapSize.x];

        
        timespec bTime;
        clock_gettime(CLOCK_BOOTTIME, &bTime);
        srand(bTime.tv_sec + bTime.tv_nsec);

        while(mineCount--){
            int x = rand() % mapSize.x;
            int y = rand() % mapSize.y;
            
            int i = 8;
            while(tiles[y][x].type == MineTile && i--){ // Do not place a mine on a tile that already has a mine, try 8 times
                x = rand() % mapSize.x;
                y = rand() % mapSize.y;
            }

            tiles[y][x].type = MineTile;
        }

        for(int y = 0; y < mapSize.y; y++){
            for(int x = 0; x < mapSize.x; x++){
                Tile& tile = tiles[y][x];

                tile.hidden = true;
                tile.flagged = false;

                if(tile.type == MineTile)
                    continue; // Don't calculate surrounding mines for mine tiles

                for(int i = std::max(0, y - 1); i <= y + 1 && i < mapSize.y; i++){
                    if(x - 1 >= 0 && tiles[i][x - 1].type == MineTile){
                        tile.surroundingMines++;
                    }

                    if(tiles[i][x].type == MineTile){
                        tile.surroundingMines++;
                    }

                    if(x + 1 < mapSize.x && tiles[i][x + 1].type == MineTile){
                        tile.surroundingMines++;
                    }
                }
            }
        }
    }

    void Paint(surface_t* surface){
        for(int y = 0; y < mapSize.y; y++){
            for(int x = 0; x < mapSize.x; x++){
                Tile& tile = tiles[y][x];

                if(tile.hidden){
                    if(tile.flagged){
                        Lemon::Graphics::surfacecpy(surface, &resources, {fixedBounds.x + x * 16, fixedBounds.y + y * 16}, {16, 16, 16, 16}); // Flagged tile
                    } else {
                        Lemon::Graphics::surfacecpy(surface, &resources, {fixedBounds.x + x * 16, fixedBounds.y + y * 16}, {0, 16, 16, 16}); // Hidden tile
                    }
                } else {
                    if(tile.flagged){
                        if(tile.type == MineTile){
                            Lemon::Graphics::surfacecpy(surface, &resources, {fixedBounds.x + x * 16, fixedBounds.y + y * 16}, {16, 16, 16, 16}); // Flagged tile
                        } else {
                            Lemon::Graphics::surfacecpy(surface, &resources, {fixedBounds.x + x * 16, fixedBounds.y + y * 16}, {16 * 3, 16, 16, 16}); // Revealed, player fucked up and flagged tile that is not a mine
                        }
                    } else {
                        if(tile.type == MineTile){
                            Lemon::Graphics::surfacecpy(surface, &resources, {fixedBounds.x + x * 16, fixedBounds.y + y * 16}, {16 * 2, 16, 16, 16}); // Mine tile
                        } else if(tile.type == EmptyTile){
                            Lemon::Graphics::surfacecpy(surface, &resources, {fixedBounds.x + x * 16, fixedBounds.y + y * 16}, {16 * tile.surroundingMines, 0, 16, 16}); // Empty/numbered tile
                        }
                    }
                }
            }
        }
    }

    void OnMouseUp(vector2i_t pos){
        pos -= fixedBounds.pos;
        int tileX = pos.x / 16;
        int tileY = pos.y / 16;

        if(tileX < 0 || tileX >= mapSize.x || tileY < 0 || tileY > mapSize.y) return;

        Tile& tile = tiles[tileY][tileX];

        if(tile.flagged){
            return; // Don't reveal flagged tiles
        }

        std::function<void(int,int)> revealAdjacent = [&](int x, int y) {
            for(int i = std::max(0, y - 1); i <= y + 1 && i < mapSize.y; i++){
                if(x - 1 >= 0 && !tiles[i][x - 1].flagged){ // If something has been incorrectly flagged make sure nto to reveal
                    bool wasHidden = tiles[i][x - 1].hidden;
                    tiles[i][x - 1].hidden = false;
                    if(tiles[i][x - 1].surroundingMines == 0 && wasHidden){ // If tile does not have a number reveal more tiles
                        revealAdjacent(x - 1, i);
                    }
                }

                if(!tiles[i][x].flagged){
                    bool wasHidden = tiles[i][x].hidden;
                    tiles[i][x].hidden = false;
                    if(tiles[i][x].surroundingMines == 0 && wasHidden){
                        revealAdjacent(x, i);
                    }
                }

                if(x + 1 < mapSize.x && !tiles[i][x + 1].flagged){
                    bool wasHidden = tiles[i][x + 1].hidden;
                    tiles[i][x + 1].hidden = false;
                    if(tiles[i][x + 1].surroundingMines == 0 && wasHidden){
                        revealAdjacent(x + 1, i);
                    }
                    tiles[i][x + 1].hidden = false;
                }
            }
        };

        if(tile.type == MineTile){
            GameOver();
        } else {
            tile.hidden = false;
            if(tile.surroundingMines == 0){
                revealAdjacent(tileX, tileY);
            }
        }
    }

    void OnRightMouseUp(vector2i_t pos){
        pos -= fixedBounds.pos;
        int x = pos.x / 16;
        int y = pos.y / 16;

        if(x < 0 || x >= mapSize.x || y < 0 || y > mapSize.y) return;

        Tile& tile = tiles[y][x];

        if(!tile.hidden) return; // Only hidden tiles can be flagged

        tile.flagged = !tile.flagged;

        CheckWin();
    }

    int GetDifficulty(){
        return difficulty;
    }
};

MinesweeperGame game(EasyDifficulty);

enum {
    MenuEasy,
    MenuMedium,
    MenuHard,
};

Lemon::GUI::WindowMenu gameMenu = {{"Game"}, {{MenuEasy, "Play Easy"}, {MenuMedium, "Play Medium"}, {MenuHard, "Play Hard"}}};

void UpdateWindowSize(){ // Update window size according to the map size
    vector2i_t mapSize = difficultySizes[game.GetDifficulty()];

    mapSize.x *= 16;
    mapSize.y = (mapSize.y * 16) + window->rootContainer.GetFixedBounds().y; // Account for menu bar

    window->Resize(mapSize);
}

bool OnWindowCommand(Lemon::LemonEvent& ev){
    switch(ev.windowCmd){
        case MenuEasy:
            game.Generate(EasyDifficulty);
            break;
        case MenuMedium:
            game.Generate(MediumDifficulty);
            break;
        case MenuHard:
            game.Generate(HardDifficulty);
            break;
    }
    UpdateWindowSize();
    return true;
}

int main(int argc, char** argv){
    window = new Lemon::GUI::Window("Minesweeper", {160, 160}, 0, Lemon::GUI::GUI);
    window->rootContainer.background = {0, 0, 0, 0};

    window->CreateMenuBar();
    window->menuBar->items.push_back(gameMenu);

    Lemon::Graphics::LoadImage("/system/lemon/resources/minesweeper.png", &resources);

    window->AddWidget(&game);

    game.Generate(EasyDifficulty);
    UpdateWindowSize();

    window->Paint();
    window->GUIRegisterEventHandler(Lemon::EventWindowCommand, OnWindowCommand);

    while(!window->closed){
	    Lemon::WindowServer::Instance()->Poll();

        window->GUIPollEvents();
        
        window->Paint();
        Lemon::WindowServer::Instance()->Wait();
    }

    delete window;

    return 0;
}