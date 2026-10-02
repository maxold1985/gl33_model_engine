#include "engine.h"
#include <cstdio>

int main()
{
    Engine engine;

    if (!engine_init(
            engine,
            GetModuleHandleA(nullptr),
            1280,
            720
        )) {
        std::fprintf(
            stderr,
            "Engine initialization failed.\n"
        );

        engine_shutdown(engine);

        std::printf(
            "Press ENTER to exit..."
        );
        std::getchar();

        return 1;
    }

    const int result =
        engine_run(engine);

    engine_shutdown(engine);
    return result;
}
