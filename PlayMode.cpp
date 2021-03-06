#include "PlayMode.hpp"

#include "Load.hpp"

//for the GL_ERRORS() macro:
#include "data_path.hpp"
#include "gl_errors.hpp"
#include "png.h"

//for glm::value_ptr() :
#include <array>
#include <bits/stdint-uintn.h>
#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <string>
#include <map>
#include <dirent.h>	
#include <fstream>

#define ENEMY_SPRITE_OFFSET 7
#define BULLET_SPRITE_OFFSET 22
#define NULL_BACKGROUND_VALUE 0b0000011111111111

std::array< PPU466::Palette, 8 > palette_table;
std::array< PPU466::Tile, 16 * 16 > tile_table;

size_t sprite_index = 0;

// helper data structure to link the tile name to tile index
std::map<std::string, size_t>name_to_index;

struct Level {
	int player_x, player_y;
	int basement_x, basement_y;
	std::vector<std::pair<int, int> >walls;
	std::vector<std::pair<int, int> >enemies;
	std::vector<std::pair<int, int> >background;
};

std::vector<Level>level_table;

Load<void> sprite_loading(LoadTagDefault, []() -> void {
	std::string path = data_path("sprites");
	printf("data_path: %s\n", path.c_str());
	DIR *dir = opendir(path.c_str());
	struct dirent *file;
	// read the sprite directory and find all files
	while ((file = readdir(dir)) != nullptr) {
		std::string sprite_name = file->d_name; 
		// read sprite files
		if (sprite_name != "." && sprite_name != "..") {
			std::ifstream sprite_file(path + '/' + sprite_name);
			if (sprite_file.is_open()) {
				std::string line;		// buffer
				int line_counter = 0;	// indexing

				std::array<std::string, 8> bit0;
				std::array<std::string, 8> bit1;

				// the first color should be fully opaque
				palette_table[sprite_index][0] = glm::u8vec4(0, 0, 0, 0);

				while (getline(sprite_file, line)) {
					// ignore the line starts with hashtag
					if (line.length() == 0 || line[0] == '#') {
						continue;
					}
					
					// 1. palette data (3lines)
					if (line_counter < 3) {
						int r, g, b, a;
						r = std::stoi(line.substr(0, 2), nullptr, 16);
						g = std::stoi(line.substr(2, 2), nullptr, 16);
						b = std::stoi(line.substr(4, 2), nullptr, 16);
						a = std::stoi(line.substr(6, 2), nullptr, 16);
						
						// load to palette
						palette_table[sprite_index][line_counter+1] = glm::u8vec4(r, g, b, a);
						// printf("palette_table[%lu][%d] = (%d, %d, %d, %d)\n", sprite_index, line_counter+1, r, g, b, a);
					} 
					// 2. tile data (8 lines)
					else {
						size_t row = line_counter - 3;
						for (int i = 0; i < 8; i++) {
							int digit = std::atoi(line.substr(i, 1).c_str());
							// bit 0
							if (digit % 2 == 0) {
								bit0[row] += "0";
							} else {
								bit0[row] += "1";
							}

							// bit 1
							if (((digit >> 1) & 1) == 0) {
								bit1[row] += "0";
							} else {
								bit1[row] += "1";
							}
						}
						// convert & strore the bits into tile_table
						tile_table[sprite_index*4].bit0[row] = std::stoi(bit0[row], nullptr, 2);
						tile_table[sprite_index*4].bit1[row] = std::stoi(bit1[row], nullptr, 2);
					}
					line_counter++;
				}

				sprite_file.close();

				// rorate the tiles (up->right->down->left)
				for (size_t ind = sprite_index*4+1; ind < (sprite_index+1)*4; ++ind) {
					for (int i = 0; i < 8; ++i) {
						for (int j = 0; j < 8; ++j) {
							tile_table[ind].bit0[i] += (tile_table[ind-1].bit0[j] & (1 << (7-i))) >> (7-i) << j;
							tile_table[ind].bit1[i] += (tile_table[ind-1].bit1[j] & (1 << (7-i))) >> (7-i) << j;
						}
					}
				}

				// build an index to map the name of sprites to the index of tile & palette
				name_to_index.insert( std::pair<std::string, size_t>(sprite_name, sprite_index));
				printf("%s ==> %lu\n", sprite_name.c_str(), sprite_index);
				sprite_index++;
			}
		}
	}


});

Load<void> levels(LoadTagDefault, []() -> void {
	std::string path = data_path("levels");
	printf("data_path: %s\n", path.c_str());
	DIR *dir = opendir(path.c_str());
	struct dirent *file;
	// read the levels
	while ((file = readdir(dir)) != nullptr) {
		std::string level_name = file->d_name; 
		// read level files
		if (level_name != "." && level_name != "..") {
			Level level;
			std::ifstream level_file(path + '/' + level_name);
			if (level_file.is_open()) {
				std::string line;
				size_t row = 0;
				while (getline(level_file, line)) {
					for (unsigned i = 0; i < line.length(); i++) {
						if (line[i] == 'p') {
							level.player_x = i;
							level.player_y = row;
						} else if (line[i] == 'b') {
							level.basement_x = i;
							level.basement_y = row;
						} else if (line[i] == 'w') {
							level.walls.push_back(std::pair<int, int>(i, row));
						} else if (line[i] == 'e') {
							level.enemies.push_back(std::pair<int, int>(i, row));
						} else if (line[i] == 'o') {
							level.background.push_back(std::pair<int, int>(row, i));
						}
					}
					row++;
				}
			}
			level_table.push_back(level);
		}
	}
});

void PlayMode::initialize_level(int level) {
	ppu.tile_table = tile_table;
	ppu.palette_table = palette_table;

	int y_offset = 0;
	int tile_offset = 8;

	// load level 0
	// 1. set player to sprites[0]
	ppu.sprites[level].index = name_to_index["player"] * 4;
	ppu.sprites[level].attributes = name_to_index["player"];
	player.pos.x = level_table[level].player_x * tile_offset;
	player.pos.y = level_table[level].player_y * tile_offset + y_offset;

	// 2. set basement to sprites[1]
	ppu.sprites[1].index = name_to_index["basement"] * 4;
	ppu.sprites[1].attributes = name_to_index["basement"];
	ppu.sprites[1].x = level_table[level].basement_x * tile_offset;
	ppu.sprites[1].y = level_table[level].basement_y * tile_offset + y_offset;

	// 3. walls [2-6] -> only the walls around basement is destroyable 
	size_t index = 2;
	for (std::vector<std::pair<int, int> >::iterator it = level_table[level].walls.begin(); 
		 it != level_table[level].walls.end(); ++it) {
			ppu.sprites[index].index = name_to_index["wall"] * 4;
			ppu.sprites[index].attributes = name_to_index["wall"];
			ppu.sprites[index].x = it->first * tile_offset;
			ppu.sprites[index].y = it->second * tile_offset + y_offset;

			index++;
		}


	// 4. enemies [7-21]
	for (index = ENEMY_SPRITE_OFFSET; index < 22; ++index) {
		ppu.sprites[index].index = name_to_index["enemy"] * 4;
		ppu.sprites[index].attributes = name_to_index["enemy"];
	}
	for (std::vector<std::pair<int, int> >::iterator it = level_table[level].enemies.begin(); 
		 it != level_table[level].enemies.end(); ++it) {
			Tank enemy;
			enemy.pos.x = it->first * tile_offset;
			enemy.pos.y = it->second * tile_offset;
			int randint = rand() % 4;
			if (randint == 0) { // up
				enemy.direction = glm::vec2(0, 1);
			} else if (randint == 1) { // right
				enemy.direction = glm::vec2(1, 0);
			} else if (randint == 1) { // down
				enemy.direction = glm::vec2(0, -1);
			} else { // left
				enemy.direction = glm::vec2(-1, 0);
			}
			enemies.push_back(enemy);
	}


	// 5. bullets [22-63] -> initially all of them are out of screen
	for (index = BULLET_SPRITE_OFFSET; index < 64; ++index) {
		ppu.sprites[index].attributes = name_to_index["bullet"];
		Bullet bullet;
		bullet.pos = glm::vec2(255, 255);
		bullet.direction = glm::vec2(0, 0);
		bullets.push_back(bullet);
	}

	// 6. background
	for (size_t i = 0; i < PPU466::BackgroundWidth * PPU466::BackgroundHeight; ++i) {
		ppu.background[i] = NULL_BACKGROUND_VALUE;
	}
	uint16_t background_value = (name_to_index["wall"] << 8) + name_to_index["wall"] * 4;
	for (size_t i = 0; i < level_table[level].background.size(); ++i) {
		int row = level_table[level].background[i].first;
		int col = level_table[level].background[i].second;
		ppu.background[row * PPU466::BackgroundWidth + col] = background_value;
	}
}

PlayMode::PlayMode() {
	initialize_level(0);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_LEFT) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.downs += 1;
			space.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_LEFT) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = false;
			return true;
		}
	}

	return false;
}

// decide if the given sprite collides with something else
int PlayMode::check_collision(glm::vec2 sprite, size_t sprite_index, std::array<PPU466::Sprite, 64> *sprites, int width) {
	// check collision with other sprites
	for (size_t i = 0; i < BULLET_SPRITE_OFFSET; i++ ) {
		if (i != sprite_index &&
			sprite.x < (*sprites)[i].x + (width) && 
			((*sprites)[i].x) < (sprite.x + (width)) && 
			sprite.y < (*sprites)[i].y + (width) && 
			(*sprites)[i].y < (sprite.y + (width))) {
				if (sprite.x != (*sprites)[i].x && sprite.y != (*sprites)[i].y) // not the same sprite
					return i;
			}
	}

	// check collision with background
	int col = sprite.x / 8;
	int row = sprite.y / 8;
	for (int i = row; i <= row + 1; ++i) {
		for (int j = col; j <= col + 1; ++j) {
			if (i < 0 || j < 0) {
				continue;
			}
			int index = i * PPU466::BackgroundWidth + j;
			if (ppu.background[index] != NULL_BACKGROUND_VALUE) {
				return -index;
			}
		}
	}
	// no collision
	return sprite_index;
}

// return game_over
bool PlayMode::hit_by_bullet(int collision_index, Tank &player, 
				   std::array<PPU466::Sprite, 64> &sprites, 
				   std::vector<Tank>&enemies) {
	if (collision_index == 0) { // player -> reset to 0, 0
		//player.pos.x = 0;
		//player.pos.y = 0;
		return false;
	} else if (collision_index == 1) {
		// basement is destroyed
		return true;
	}
	else { // enemies or wall, remove from screen
		if (collision_index < ENEMY_SPRITE_OFFSET) { // wall
			sprites[collision_index].x = 255;
			sprites[collision_index].y = 255;
		} else { // enemies
			enemies[collision_index-ENEMY_SPRITE_OFFSET].pos.x = 255;
			enemies[collision_index-ENEMY_SPRITE_OFFSET].pos.y = 255;
			enemies[collision_index-ENEMY_SPRITE_OFFSET].direction.x = 0;
			enemies[collision_index-ENEMY_SPRITE_OFFSET].direction.y = 0;
		}
		return false;
	}
}

void PlayMode::move_tank(Tank &tank, int index, float speed, float elapsed) {

	tank.pos.x += speed * elapsed *tank.direction.x;
	tank.pos.y += speed * elapsed * tank.direction.y;

	// bounding to screen
	tank.pos.x = std::fmax(0, tank.pos.x);
	tank.pos.x = std::fmin(tank.pos.x, PPU466::ScreenWidth-8);
	tank.pos.y = std::fmax(0, tank.pos.y);
	tank.pos.y = std::fmin(tank.pos.y, PPU466::ScreenHeight-8);

	// if the tank collide with other sprites, reset it's position to avoid collision
	int collision_index = check_collision(tank.pos, index, &ppu.sprites, 8);
	if (collision_index != index) {
		uint8_t sp_x, sp_y;
		if (collision_index > 0) { // collision with sprites
			sp_x = ppu.sprites[collision_index].x;
			sp_y = ppu.sprites[collision_index].y;

			if (tank.direction.x == 0) {
				tank.pos.y = sp_y - 8 * tank.direction.y;
			} else {
				tank.pos.x = sp_x - 8* tank.direction.x;
			}

		} else { // collision with background
			collision_index = (-collision_index);
			sp_x = collision_index % PPU466::BackgroundWidth * 8;
			sp_y = floor(collision_index / PPU466::BackgroundWidth) * 8;
			// printf("detected collision at: %d -> (%d, %.1f) -> (%d, %d)\n", 
			// 		collision_index, collision_index % PPU466::BackgroundWidth, 
			// 		floor(collision_index / PPU466::BackgroundWidth),
			// 		sp_x, sp_y);

			if (tank.direction.x == 0) {
				// if tank is going up, ignore the sprite overlap at the upper
				if (!(tank.direction.y == 1 && sp_y < (tank.pos.y-8)) && 
					!(tank.direction.y == -1 && sp_y > tank.pos.y)) {
					int diff = 8 - std::abs(tank.pos.y - sp_y);
					tank.pos.y -= tank.direction.y * diff;
				}
					
			} else {
				if (!(tank.direction.x == 1 && sp_x < tank.pos.x) && 
					!(tank.direction.x == -1 && sp_x > (tank.pos.x-8))) {
					int diff = 8 - std::abs(tank.pos.x - sp_x);
					tank.pos.x -= tank.direction.x * diff;
				}
			}
		}	
	}	
}

void PlayMode::emit_bullet(Tank &tank) {
	// 1. find the next available bullet sprite
	size_t index;
	for (index = 0; index < bullets.size(); ++index) {
		if (bullets[index].direction.x == 0 && bullets[index].direction.y == 0) {
			break;
		}
	}
	// 2. emit the bullet
	bullets[index].direction.x = tank.direction.x;
	bullets[index].direction.y = tank.direction.y;
	if (tank.direction.x == 0) {
		if (tank.direction.y == 1) // up
			ppu.sprites[BULLET_SPRITE_OFFSET + index].index = name_to_index["bullet"] * 4;
		else // down
			ppu.sprites[BULLET_SPRITE_OFFSET + index].index = name_to_index["bullet"] * 4 + 2;
	} else if (tank.direction.x == 1) { // right
		ppu.sprites[BULLET_SPRITE_OFFSET + index].index = name_to_index["bullet"] * 4 + 1;
	} else { // left
		ppu.sprites[BULLET_SPRITE_OFFSET + index].index = name_to_index["bullet"] * 4 + 3;
	}

	bullets[index].pos.x = tank.pos.x;
	bullets[index].pos.y = tank.pos.y;
	space.pressed = false;
}

void PlayMode::update(float elapsed) {
	// 1. player's move
	constexpr float PlayerSpeed = 30.0f;
	if (left.pressed) {
		player.direction.x = -1;
		player.direction.y = 0;
		ppu.sprites[0].index = name_to_index["player"] + 3;
		move_tank(player, 0, PlayerSpeed, elapsed);
	} 
	else if (right.pressed) {
		player.direction.x = 1;
		player.direction.y = 0;
		ppu.sprites[0].index = name_to_index["player"] + 1;
		move_tank(player, 0, PlayerSpeed, elapsed);
	}
	else if (down.pressed) {
		player.direction.x = 0;
		player.direction.y = -1;
		ppu.sprites[0].index = name_to_index["player"] + 2;
		move_tank(player, 0, PlayerSpeed, elapsed);
	}
	else if (up.pressed) {
		player.direction.x = 0;
		player.direction.y = 1;
		ppu.sprites[0].index = name_to_index["player"];
		move_tank(player, 0, PlayerSpeed, elapsed);
	}


	// 2. emit a bullet when space is pressed
	if (space.pressed) {
		// 1. find the next available bullet sprite
		size_t index;
		for (index = 0; index < bullets.size(); ++index) {
			if (bullets[index].direction.x == 0 && bullets[index].direction.y == 0) {
				break;
			}
		}
		// 2. emit the bullet
		emit_bullet(player);
		space.pressed = false;
	}


	// 3. enemies -> change the direction randomly
	for (size_t i = 0; i < enemies.size(); ++i) {
		if (enemies[i].direction.x == 0 && enemies[i].direction.y == 0) // ignore `dead` enemies
			continue;
		int randint = rand() % 20;
		if (randint == 1) {
			// change the direction
			randint = rand() % 4;
			if (randint == 0) { // up
				enemies[i].direction.x = 0;
				enemies[i].direction.y = 1;
				ppu.sprites[i+ENEMY_SPRITE_OFFSET].index = name_to_index["enemy"]*4;
			} else if (randint == 1) { // right
				enemies[i].direction.x = 1;
				enemies[i].direction.y = 0;
				ppu.sprites[i+ENEMY_SPRITE_OFFSET].index = name_to_index["enemy"]*4 + 1;
			} else if (randint == 1) { // down
				enemies[i].direction.x = 0;
				enemies[i].direction.y = -1;
				ppu.sprites[i+ENEMY_SPRITE_OFFSET].index = name_to_index["enemy"]*4 + 2;
			} else { // left
				enemies[i].direction.x = -1;
				enemies[i].direction.y = 0;
				ppu.sprites[i+ENEMY_SPRITE_OFFSET].index = name_to_index["enemy"]*4 + 3;
			}
		}

		// hit randomly
		randint = rand() % 1000;
		if (randint == 1) {
			emit_bullet(enemies[i]);
		}
		// let it move!
		move_tank(enemies[i], i+ENEMY_SPRITE_OFFSET, PlayerSpeed, elapsed);
	}


	// update bullets position
	constexpr float BulletSpeed = 3.0f;
	for (int i = 0; i < int(bullets.size()); ++i) {
		if (bullets[i].direction.x != 0 || bullets[i].direction.y != 0) {
			bullets[i].pos.x += BulletSpeed * bullets[i].direction.x;
			bullets[i].pos.y += BulletSpeed * bullets[i].direction.y;
			// printf("bullet (%.1f, %.1f)\n", bullets[i].pos.x, bullets[i].pos.y);
			
			
			// check collision
			glm::vec2 b(bullets[i].pos.x, bullets[i].pos.y);
			int collision_index = check_collision(b, i, &ppu.sprites, 4);
			if (collision_index != i) {
				if (collision_index > 0)
					game_over = hit_by_bullet(collision_index, player, ppu.sprites, enemies);
				// the bullet should be disappear
				bullets[i].direction.x = 0;
				bullets[i].direction.y = 0;
				bullets[i].pos.x = 255;
				bullets[i].pos.y = 255;
			}
		}
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	space.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//--- set ppu state based on game state ---

	//background color will be some hsv-like fade:
	ppu.background_color = glm::u8vec4(0x00, 0x00, 0x00,0xff);

	//tilemap gets recomputed every frame as some weird plasma thing:
	//NOTE: don't do this in your game! actually make a map or something :-)
	for (uint32_t y = 0; y < PPU466::BackgroundHeight; ++y) {
		for (uint32_t x = 0; x < PPU466::BackgroundWidth; ++x) {
			//TODO: make weird plasma thing
			//ppu.background[x+PPU466::BackgroundWidth*y] = NULL_BACKGROUND_VALUE;// ((x+y)%16);
		}
	}

	//player sprite:
	ppu.sprites[0].x = int32_t(player.pos.x);
	ppu.sprites[0].y = int32_t(player.pos.y);

	//enemy sprites:
	for (size_t i = 0; i < enemies.size(); ++i) {
		ppu.sprites[ENEMY_SPRITE_OFFSET+i].x = enemies[i].pos.x;
		ppu.sprites[ENEMY_SPRITE_OFFSET+i].y = enemies[i].pos.y;
	}

	//bullet sprites:
	for (size_t i = 0; i < bullets.size(); ++i) {
		// reset the out-of-sight bullets
		if (bullets[i].pos.x < 0 || bullets[i].pos.x > 255 || 
			bullets[i].pos.y < 0 || bullets[i].pos.y > 255) {
			bullets[i].direction.x = 0;
			bullets[i].direction.y = 0;
			bullets[i].pos.x = 255;
			bullets[i].pos.y = 255;
		}

		ppu.sprites[BULLET_SPRITE_OFFSET+i].x = bullets[i].pos.x;
		ppu.sprites[BULLET_SPRITE_OFFSET+i].y = bullets[i].pos.y;
	}

	//--- actually draw ---
	ppu.draw(drawable_size);
}
