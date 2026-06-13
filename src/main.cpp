#include "core/Engine.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
  // Instantiate core ForgeIDE engine
  forge::Engine engine;

  // Initialize graphics window (1280x720)
  if (!engine.init(1280, 720, "ForgeIDE")) {
    std::cerr << "Fatal Error: Failed to initialize ForgeIDE core engine."
              << std::endl;
    return -1;
  }

  // Start main game loop (144 FPS VSync target)
  engine.run();

  // Graceful release of hardware and process contexts
  engine.shutdown();

  return 0;
}
