#include "PlayMode.hpp"

#include "Load.hpp"

//for the GL_ERRORS() macro:
#include "data_path.hpp"
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <array>
#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <string>
#include <map>
#include <dirent.h>	
#include <fstream>

std::array< PPU466::Palette, 8 > palette_table;
std::array< PPU466::Tile, 16 * 16 > tile_table;

size_t sprite_index = 0;

// helper data structure to link the tile name to tile index
std::map<std::string, size_t>name_to_index;

struct Level {
	int player_x, player_y;
	int basement_x, basement_y;
	std::vector<std::pair<int, int> >walls;
	std::vector<std::pair<int, int> >enermies;
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
							level.enermies.push_back(std::pair<int, int>(i, row));
						}
					}
					row++;
				}
			}
			level_table.push_back(level);
		}
	}
});

PlayMode::PlayMode() {
	//TODO:
	// you *must* use an asset pipeline of some sort to generate tiles.
	// don't hardcode them like this!
	// or, at least, if you do hardcode them like this,
	//  make yourself a script that spits out the code that you paste in here
	//   and check that script into your repository.

	//Also, *don't* use these tiles in your game:

	ppu.tile_table = tile_table;
	ppu.palette_table = palette_table;

	int y_offset = 0;
	int tile_offset = 8;

	// load level 0
	// 1. set player to sprites[0]
	ppu.sprites[0].index = name_to_index["player"] * 4 + 1;
	ppu.sprites[0].attributes = name_to_index["player"];
	ppu.sprites[0].x = level_table[0].player_x * tile_offset;
	ppu.sprites[0].y = level_table[0].player_y * tile_offset + y_offset;
	printf("player at (%d, %d)\n", ppu.sprites[0].x, ppu.sprites[0].y);

	// 2. set basement to sprites[1]
	ppu.sprites[1].index = name_to_index["basement"] * 4;
	ppu.sprites[1].attributes = name_to_index["basement"];
	ppu.sprites[1].x = level_table[0].basement_x * tile_offset;
	ppu.sprites[1].y = level_table[0].basement_y * tile_offset + y_offset;

	printf("basement at (%d, %d)\n", ppu.sprites[1].x, ppu.sprites[1].y);

	// 3. enermies [2-15]
	printf("name_to_index=%lu\n", name_to_index["enermy"]);
	size_t index = 2;
	for (std::vector<std::pair<int, int> >::iterator it = level_table[0].enermies.begin(); 
		 it != level_table[0].enermies.end(); it++) {
			ppu.sprites[index].index = name_to_index["enermy"] * 4;
			ppu.sprites[index].attributes = name_to_index["enermy"];
			ppu.sprites[index].x = it->first * tile_offset;
			ppu.sprites[index].y = it->second * tile_offset + y_offset;

			printf("enermies at (%d, %d) attr=%d\n", ppu.sprites[index].x, ppu.sprites[index].y, ppu.sprites[index].attributes);

			index++;
		}

	// tile_offset. walls [16-63]
	index = 16;
	for (std::vector<std::pair<int, int> >::iterator it = level_table[0].walls.begin(); 
		 it != level_table[0].walls.end(); it++) {
			ppu.sprites[index].index = name_to_index["wall"] * 4;
			ppu.sprites[index].attributes = name_to_index["wall"];
			ppu.sprites[index].x = it->first * tile_offset;
			ppu.sprites[index].y = it->second * tile_offset + y_offset;

			printf("walls at (%d, %d)\n", ppu.sprites[index].x, ppu.sprites[index].y);

			index++;
		}
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
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	// (will be used to set background color)
	background_fade += elapsed / 10.0f;
	background_fade -= std::floor(background_fade);

	constexpr float PlayerSpeed = 30.0f;
	if (left.pressed) {
		player_at.x -= PlayerSpeed * elapsed;
		ppu.sprites[0].index = name_to_index["player"] + 3;
	} 
	if (right.pressed) {
		player_at.x += PlayerSpeed * elapsed;
		ppu.sprites[0].index = name_to_index["player"] + 1;
	}
	if (down.pressed) {
		player_at.y -= PlayerSpeed * elapsed;
		ppu.sprites[0].index = name_to_index["player"] + 2;
	}
	if (up.pressed) {
		player_at.y += PlayerSpeed * elapsed;
		ppu.sprites[0].index = name_to_index["player"];
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//--- set ppu state based on game state ---

	//background color will be some hsv-like fade:
	ppu.background_color = glm::u8vec4(
		std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (background_fade + 0.0f / 3.0f) ) ) ))),
		std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (background_fade + 1.0f / 3.0f) ) ) ))),
		std::min(255,std::max(0,int32_t(255 * 0.5f * (0.5f + std::sin( 2.0f * M_PI * (background_fade + 2.0f / 3.0f) ) ) ))),
		0xff
	);

	//tilemap gets recomputed every frame as some weird plasma thing:
	//NOTE: don't do this in your game! actually make a map or something :-)
	for (uint32_t y = 0; y < PPU466::BackgroundHeight; ++y) {
		for (uint32_t x = 0; x < PPU466::BackgroundWidth; ++x) {
			//TODO: make weird plasma thing
			ppu.background[x+PPU466::BackgroundWidth*y] = 0b0000011111111111;// ((x+y)%16);
		}
	}

	//background scroll:
	//ppu.background_position.x = int32_t(-0.5f * player_at.x);
	//ppu.background_position.y = int32_t(-0.5f * player_at.y);

	//player sprite:
	ppu.sprites[0].x = int32_t(player_at.x);
	ppu.sprites[0].y = int32_t(player_at.y);

	//--- actually draw ---
	ppu.draw(drawable_size);
}
