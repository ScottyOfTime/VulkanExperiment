#include "game.h"

int main(int argc, char* argv[])
{
	Game g;
	// What if init() fails?
	g.init();
	g.run();
	g.deinit();
}
