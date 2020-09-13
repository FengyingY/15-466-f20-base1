#include "PPU466.hpp"
#include "Mode.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, space;

	//player position:
	struct Tank {
		glm::vec2 pos;
		glm::vec2 direction;
		Tank(){};

		Tank(glm::vec2 p, glm::vec2 d) {
			pos = p;
			direction = d;
		}
	};

	struct Tank player;

	//----- drawing handled by PPU466 -----
	PPU466 ppu;

	// enemies position
	std::vector<struct Tank>enemies;

	// bullet position
	struct Bullet {
		glm::vec2 pos;
		glm::vec2 direction;
	};
	std::vector<Bullet> bullets;

	bool game_over = false;

	bool hit_by_bullet(int collision_index, struct Tank &player, 
			std::array<PPU466::Sprite, 64> &sprites, 
			std::vector<struct Tank>&enemies);

	int check_collision(glm::vec2 sprite, size_t sprite_index, 
				        std::array<PPU466::Sprite, 64> *sprites, int width);
};
