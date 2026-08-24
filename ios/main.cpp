#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

struct EngineSimIOSState
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;

    int width = 0;
    int height = 0;

    float throttle = 0.30f;
    float rpm = 850.0f;
    float displayedRpm = 850.0f;

    Uint64 previousTicks = 0;
};

static constexpr float Pi = 3.14159265358979323846f;

static void drawFilledRect(
    SDL_Renderer *renderer,
    float x,
    float y,
    float width,
    float height,
    Uint8 r,
    Uint8 g,
    Uint8 b,
    Uint8 a = 255)
{
    SDL_FRect rect {
        x,
        y,
        width,
        height
    };

    SDL_SetRenderDrawColor(
        renderer,
        r,
        g,
        b,
        a
    );

    SDL_RenderFillRect(
        renderer,
        &rect
    );
}

static void drawLine(
    SDL_Renderer *renderer,
    float x1,
    float y1,
    float x2,
    float y2,
    Uint8 r,
    Uint8 g,
    Uint8 b,
    Uint8 a = 255)
{
    SDL_SetRenderDrawColor(
        renderer,
        r,
        g,
        b,
        a
    );

    SDL_RenderLine(
        renderer,
        x1,
        y1,
        x2,
        y2
    );
}

static void drawCircle(
    SDL_Renderer *renderer,
    float centerX,
    float centerY,
    float radius,
    Uint8 r,
    Uint8 g,
    Uint8 b)
{
    constexpr int Segments = 96;

    float previousX =
        centerX + radius;

    float previousY =
        centerY;

    for (int i = 1; i <= Segments; ++i)
    {
        const float angle =
            static_cast<float>(i)
            / static_cast<float>(Segments)
            * 2.0f
            * Pi;

        const float x =
            centerX
            + std::cos(angle)
            * radius;

        const float y =
            centerY
            + std::sin(angle)
            * radius;

        drawLine(
            renderer,
            previousX,
            previousY,
            x,
            y,
            r,
            g,
            b
        );

        previousX = x;
        previousY = y;
    }
}

static void drawTachometer(
    EngineSimIOSState *state)
{
    SDL_Renderer *renderer =
        state->renderer;

    const float width =
        static_cast<float>(state->width);

    const float height =
        static_cast<float>(state->height);

    const float centerX =
        width * 0.34f;

    const float centerY =
        height * 0.52f;

    const float radius =
        std::min(width, height)
        * 0.27f;

    //
    // Outer tach rings
    //

    drawCircle(
        renderer,
        centerX,
        centerY,
        radius,
        170,
        170,
        175
    );

    drawCircle(
        renderer,
        centerX,
        centerY,
        radius - 4.0f,
        55,
        55,
        60
    );

    //
    // Tach markings
    //
    // Roughly 240 degrees of sweep.
    //

    constexpr int MajorTicks = 9;

    const float startAngle =
        140.0f * Pi / 180.0f;

    const float sweepAngle =
        260.0f * Pi / 180.0f;

    for (int i = 0; i < MajorTicks; ++i)
    {
        const float t =
            static_cast<float>(i)
            / static_cast<float>(MajorTicks - 1);

        const float angle =
            startAngle
            + sweepAngle
            * t;

        const float innerRadius =
            radius * 0.78f;

        const float outerRadius =
            radius * 0.94f;

        const float x1 =
            centerX
            + std::cos(angle)
            * innerRadius;

        const float y1 =
            centerY
            + std::sin(angle)
            * innerRadius;

        const float x2 =
            centerX
            + std::cos(angle)
            * outerRadius;

        const float y2 =
            centerY
            + std::sin(angle)
            * outerRadius;

        drawLine(
            renderer,
            x1,
            y1,
            x2,
            y2,
            210,
            210,
            215
        );
    }

    //
    // Redline region
    //

    for (int i = 0; i < 16; ++i)
    {
        const float t =
            0.79f
            + 0.21f
            * static_cast<float>(i)
            / 15.0f;

        const float angle =
            startAngle
            + sweepAngle
            * t;

        const float innerRadius =
            radius * 0.82f;

        const float outerRadius =
            radius * 0.93f;

        drawLine(
            renderer,

            centerX
                + std::cos(angle)
                * innerRadius,

            centerY
                + std::sin(angle)
                * innerRadius,

            centerX
                + std::cos(angle)
                * outerRadius,

            centerY
                + std::sin(angle)
                * outerRadius,

            220,
            55,
            55
        );
    }

    //
    // Tach needle
    //

    const float normalizedRpm =
        std::clamp(
            state->displayedRpm / 8000.0f,
            0.0f,
            1.0f
        );

    const float needleAngle =
        startAngle
        + sweepAngle
        * normalizedRpm;

    const float needleLength =
        radius * 0.76f;

    drawLine(
        renderer,

        centerX,
        centerY,

        centerX
            + std::cos(needleAngle)
            * needleLength,

        centerY
            + std::sin(needleAngle)
            * needleLength,

        245,
        245,
        245
    );

    //
    // Needle hub
    //

    drawFilledRect(
        renderer,

        centerX - 5.0f,
        centerY - 5.0f,

        10.0f,
        10.0f,

        235,
        235,
        235
    );
}

static void drawThrottle(
    EngineSimIOSState *state)
{
    SDL_Renderer *renderer =
        state->renderer;

    const float width =
        static_cast<float>(state->width);

    const float height =
        static_cast<float>(state->height);

    const float x =
        width * 0.72f;

    const float y =
        height * 0.23f;

    const float barWidth =
        width * 0.055f;

    const float barHeight =
        height * 0.55f;

    //
    // Throttle background
    //

    drawFilledRect(
        renderer,
        x,
        y,
        barWidth,
        barHeight,
        30,
        30,
        35
    );

    //
    // Throttle fill
    //

    const float fillHeight =
        barHeight
        * state->throttle;

    drawFilledRect(
        renderer,

        x,
        y + barHeight - fillHeight,

        barWidth,
        fillHeight,

        185,
        185,
        190
    );

    //
    // Border
    //

    drawLine(
        renderer,
        x,
        y,
        x + barWidth,
        y,
        100,
        100,
        105
    );

    drawLine(
        renderer,
        x,
        y + barHeight,
        x + barWidth,
        y + barHeight,
        100,
        100,
        105
    );

    drawLine(
        renderer,
        x,
        y,
        x,
        y + barHeight,
        100,
        100,
        105
    );

    drawLine(
        renderer,
        x + barWidth,
        y,
        x + barWidth,
        y + barHeight,
        100,
        100,
        105
    );
}

static void drawEnginePulse(
    EngineSimIOSState *state)
{
    const float width =
        static_cast<float>(state->width);

    const float height =
        static_cast<float>(state->height);

    const float frequency =
        state->displayedRpm / 60.0f;

    const float time =
        static_cast<float>(
            SDL_GetTicks()
        ) / 1000.0f;

    const float pulse =
        0.5f
        + 0.5f
        * std::sin(
            time
            * frequency
            * 2.0f
            * Pi
        );

    const float pulseWidth =
        width
        * (0.08f + 0.08f * pulse);

    drawFilledRect(
        state->renderer,

        width * 0.69f,
        height * 0.84f,

        pulseWidth,
        6.0f,

        200,
        200,
        205
    );
}

static void updateSimulation(
    EngineSimIOSState *state)
{
    const Uint64 currentTicks =
        SDL_GetTicks();

    if (state->previousTicks == 0)
    {
        state->previousTicks =
            currentTicks;

        return;
    }

    const float deltaTime =
        static_cast<float>(
            currentTicks
            - state->previousTicks
        ) / 1000.0f;

    state->previousTicks =
        currentTicks;

    //
    // Fake RPM response for the SDL/iOS graphics milestone.
    //
    // This gets replaced by the actual EngineSim simulator output.
    //

    const float targetRpm =
        850.0f
        + state->throttle
        * 6800.0f;

    const float response =
        1.0f
        - std::exp(
            -deltaTime
            * 5.0f
        );

    state->rpm +=
        (targetRpm - state->rpm)
        * response;

    //
    // Slight mechanical-looking flutter.
    //

    const float flutter =
        std::sin(
            static_cast<float>(
                currentTicks
            )
            * 0.015f
        )
        * 20.0f;

    state->displayedRpm =
        state->rpm
        + flutter;
}

static void updateThrottleFromTouch(
    EngineSimIOSState *state,
    float normalizedY)
{
    //
    // SDL finger coordinates use 0 at the top.
    //

    state->throttle =
        std::clamp(
            1.0f - normalizedY,
            0.0f,
            1.0f
        );
}

SDL_AppResult SDL_AppInit(
    void **appstate,
    int argc,
    char *argv[])
{
    (void)argc;
    (void)argv;

    std::printf(
        "========================================\n"
        " EngineSim SDL3 iOS host starting\n"
        "========================================\n"
    );

    if (!SDL_Init(
        SDL_INIT_VIDEO
        | SDL_INIT_EVENTS))
    {
        std::fprintf(
            stderr,
            "SDL_Init failed: %s\n",
            SDL_GetError()
        );

        return SDL_APP_FAILURE;
    }

    auto *state =
        new EngineSimIOSState();

    state->window =
        SDL_CreateWindow(
            "EngineSim",
            1280,
            720,
            SDL_WINDOW_HIGH_PIXEL_DENSITY
            | SDL_WINDOW_RESIZABLE
        );

    if (state->window == nullptr)
    {
        std::fprintf(
            stderr,
            "SDL_CreateWindow failed: %s\n",
            SDL_GetError()
        );

        delete state;

        return SDL_APP_FAILURE;
    }

    state->renderer =
        SDL_CreateRenderer(
            state->window,
            nullptr
        );

    if (state->renderer == nullptr)
    {
        std::fprintf(
            stderr,
            "SDL_CreateRenderer failed: %s\n",
            SDL_GetError()
        );

        SDL_DestroyWindow(
            state->window
        );

        delete state;

        return SDL_APP_FAILURE;
    }

    SDL_GetWindowSizeInPixels(
        state->window,
        &state->width,
        &state->height
    );

    SDL_SetRenderVSync(
        state->renderer,
        1
    );

    state->previousTicks =
        SDL_GetTicks();

    *appstate = state;

    std::printf(
        "SDL3 initialized successfully.\n"
        "Window: %dx%d\n"
        "Renderer: %s\n",
        state->width,
        state->height,
        SDL_GetRendererName(
            state->renderer
        )
    );

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(
    void *appstate,
    SDL_Event *event)
{
    auto *state =
        static_cast<EngineSimIOSState *>(
            appstate
        );

    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }

    if (
        event->type
            == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
        || event->type
            == SDL_EVENT_WINDOW_RESIZED
    )
    {
        SDL_GetWindowSizeInPixels(
            state->window,
            &state->width,
            &state->height
        );
    }

    if (
        event->type
            == SDL_EVENT_FINGER_DOWN
        || event->type
            == SDL_EVENT_FINGER_MOTION
    )
    {
        updateThrottleFromTouch(
            state,
            event->tfinger.y
        );
    }

    if (
        event->type
            == SDL_EVENT_MOUSE_BUTTON_DOWN
        || event->type
            == SDL_EVENT_MOUSE_MOTION
    )
    {
        float mouseX = 0.0f;
        float mouseY = 0.0f;

        SDL_GetMouseState(
            &mouseX,
            &mouseY
        );

        const float height =
            std::max(
                1,
                state->height
            );

        updateThrottleFromTouch(
            state,
            mouseY / height
        );
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(
    void *appstate)
{
    auto *state =
        static_cast<EngineSimIOSState *>(
            appstate
        );

    SDL_GetWindowSizeInPixels(
        state->window,
        &state->width,
        &state->height
    );

    updateSimulation(
        state
    );

    //
    // EngineSim dark background.
    //

    SDL_SetRenderDrawColor(
        state->renderer,
        8,
        8,
        11,
        255
    );

    SDL_RenderClear(
        state->renderer
    );

    //
    // Dashboard separator.
    //

    drawLine(
        state->renderer,

        state->width * 0.62f,
        state->height * 0.13f,

        state->width * 0.62f,
        state->height * 0.87f,

        48,
        48,
        52
    );

    drawTachometer(
        state
    );

    drawThrottle(
        state
    );

    drawEnginePulse(
        state
    );

    SDL_RenderPresent(
        state->renderer
    );

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(
    void *appstate,
    SDL_AppResult result)
{
    (void)result;

    auto *state =
        static_cast<EngineSimIOSState *>(
            appstate
        );

    if (state != nullptr)
    {
        if (state->renderer != nullptr)
        {
            SDL_DestroyRenderer(
                state->renderer
            );

            state->renderer = nullptr;
        }

        if (state->window != nullptr)
        {
            SDL_DestroyWindow(
                state->window
            );

            state->window = nullptr;
        }

        delete state;
    }

    SDL_Quit();

    std::printf(
        "EngineSim SDL3 iOS host stopped.\n"
    );
}
