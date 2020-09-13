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
	glm::vec2 player_at = glm::vec2(0.0f);
	//player direction:
	glm::vec2 player_direction = glm::vec2(0, 1);

	//----- drawing handled by PPU466 -----
	PPU466 ppu;

	// enemies position
	std::vector<glm::vec2>enemy_at;

	// bullet position
	struct Bullet {
		glm::vec2 pos;
		glm::vec2 direction;
	};
	std::vector<Bullet> bullets;

	bool game_over = false;
};
