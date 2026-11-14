#define SDL_MAIN_HANDLED

#include "app/game_app.hpp"

#include <cstdio>
#include <exception>

int main()
{
  try
  {
    scsr::GameApp app;
    app.initialize();
    return app.run();
  }
  catch (const std::exception &error)
  {
    std::fprintf(stderr, "SCSR startup/runtime error: %s\n", error.what());
    return 1;
  }
}
