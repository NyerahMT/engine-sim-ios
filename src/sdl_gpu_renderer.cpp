#include "../include/sdl_gpu_renderer.h"
#include "../include/shaders.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr int MaxVertices = 500000;
constexpr int MaxIndices = 1000000;

/*
 * ============================================================
 * Persistent renderer diagnostic logger
 * ============================================================
 *
 * Writes into the same file used by ios/main.cpp:
 *
 *     Documents/engine-sim.log
 *
 * The renderer has its own append handle so we can see exactly
 * how far SDL GPU / Metal gets before a crash or abort.
 */

FILE *g_rendererLogFile =
    nullptr;

std::uint64_t rendererLogStartMs =
    0;

std::uint64_t rendererMonotonicMilliseconds()
{
    using namespace std::chrono;

    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(
            steady_clock::now()
                .time_since_epoch())
            .count());
}

void openRendererDiagnosticLog()
{
    if (g_rendererLogFile != nullptr)
    {
        return;
    }

    const char *home =
        std::getenv("HOME");

    if (
        home == nullptr
        || home[0] == '\0')
    {
        return;
    }

    const fs::path path =
        fs::path(home)
        / "Documents"
        / "engine-sim.log";

    std::error_code error;

    fs::create_directories(
        path.parent_path(),
        error);

    g_rendererLogFile =
        std::fopen(
            path.string().c_str(),
            "a");

    if (g_rendererLogFile == nullptr)
    {
        return;
    }

    std::setvbuf(
        g_rendererLogFile,
        nullptr,
        _IONBF,
        0);

    rendererLogStartMs =
        rendererMonotonicMilliseconds();

    std::fprintf(
        g_rendererLogFile,
        "[GPU-LOG ] Renderer diagnostic logger attached.\n");

    std::fflush(
        g_rendererLogFile);
}

void closeRendererDiagnosticLog()
{
    if (g_rendererLogFile == nullptr)
    {
        return;
    }

    std::fprintf(
        g_rendererLogFile,
        "[GPU-LOG ] Renderer diagnostic logger closing.\n");

    std::fflush(
        g_rendererLogFile);

    std::fclose(
        g_rendererLogFile);

    g_rendererLogFile =
        nullptr;
}

void rendererDiagnosticLog(
    const char *format,
    ...)
{
    if (g_rendererLogFile == nullptr)
    {
        openRendererDiagnosticLog();
    }

    if (g_rendererLogFile == nullptr)
    {
        return;
    }

    const std::uint64_t now =
        rendererMonotonicMilliseconds();

    const std::uint64_t elapsed =
        now >= rendererLogStartMs
            ? now - rendererLogStartMs
            : 0;

    std::fprintf(
        g_rendererLogFile,
        "[GPU +%6llu ms] ",
        static_cast<unsigned long long>(
            elapsed));

    va_list args;

    va_start(
        args,
        format);

    std::vfprintf(
        g_rendererLogFile,
        format,
        args);

    va_end(args);

    std::fprintf(
        g_rendererLogFile,
        "\n");

    std::fflush(
        g_rendererLogFile);
}

std::vector<std::uint8_t> loadBinaryFile(
    const std::string &path)
{
    std::ifstream file(
        path,
        std::ios::binary | std::ios::ate);

    if (!file)
    {
        return {};
    }

    const std::streamsize size =
        file.tellg();

    if (size <= 0)
    {
        return {};
    }

    std::vector<std::uint8_t> data(
        static_cast<std::size_t>(
            size));

    file.seekg(0);

    if (!file.read(
        reinterpret_cast<char *>(
            data.data()),
        size))
    {
        return {};
    }

    return data;
}

SDL_GPUShaderFormat selectedShaderFormat(
    SDL_GPUDevice *device,
    const char **extension)
{
    const SDL_GPUShaderFormat formats =
        SDL_GetGPUShaderFormats(
            device);

#if defined(__APPLE__)

    if (
        (formats & SDL_GPU_SHADERFORMAT_MSL)
        != 0)
    {
        *extension =
            "msl";

        return
            SDL_GPU_SHADERFORMAT_MSL;
    }

#endif

    if (
        (formats & SDL_GPU_SHADERFORMAT_SPIRV)
        != 0)
    {
        *extension =
            "spv";

        return
            SDL_GPU_SHADERFORMAT_SPIRV;
    }

    if (
        (formats & SDL_GPU_SHADERFORMAT_DXIL)
        != 0)
    {
        *extension =
            "dxil";

        return
            SDL_GPU_SHADERFORMAT_DXIL;
    }

    if (
        (formats & SDL_GPU_SHADERFORMAT_MSL)
        != 0)
    {
        *extension =
            "msl";

        return
            SDL_GPU_SHADERFORMAT_MSL;
    }

    *extension =
        nullptr;

    return
        SDL_GPU_SHADERFORMAT_INVALID;
}

}

SdlGpuRenderer::SdlGpuRenderer()
    : m_window(nullptr),
      m_gpuDevice(nullptr),
      m_vertexBuffer(nullptr),
      m_indexBuffer(nullptr),
      m_vertexTransferBuffer(nullptr),
      m_indexTransferBuffer(nullptr),
      m_scenePipeline(nullptr),
      m_uiPipeline(nullptr),
      m_sceneTexture(nullptr),
      m_depthTexture(nullptr),
      m_sceneTextureWidth(0),
      m_sceneTextureHeight(0),
      m_depthTextureWidth(0),
      m_depthTextureHeight(0),
      m_vertices(nullptr),
      m_indices(nullptr),
      m_vertexCount(0),
      m_indexCount(0),
      m_clearColor(
          0.055f,
          0.063f,
          0.071f,
          1.0f),
      m_sceneViewportX(0.0f),
      m_sceneViewportY(0.0f),
      m_sceneViewportWidth(0.0f),
      m_sceneViewportHeight(0.0f)
{
    openRendererDiagnosticLog();

    rendererDiagnosticLog(
        "SdlGpuRenderer constructor.");
}

SdlGpuRenderer::~SdlGpuRenderer()
{
    rendererDiagnosticLog(
        "SdlGpuRenderer destructor ENTER.");

    shutdown();

    rendererDiagnosticLog(
        "SdlGpuRenderer destructor END.");

    closeRendererDiagnosticLog();
}

bool SdlGpuRenderer::initialize(
    void *nativeWindowHandle,
    const std::string &shaderDirectory)
{
    openRendererDiagnosticLog();

    rendererDiagnosticLog(
        "initialize ENTER.");

    rendererDiagnosticLog(
        "nativeWindowHandle=%p",
        nativeWindowHandle);

    rendererDiagnosticLog(
        "shaderDirectory=%s",
        shaderDirectory.c_str());

    m_error.clear();

    m_window =
        nativeWindowHandle;

    if (m_window == nullptr)
    {
        m_error =
            "SDL window handle is null";

        rendererDiagnosticLog(
            "initialize FAIL: window is NULL.");

        return false;
    }

    rendererDiagnosticLog(
        "SDL_CreateGPUDevice BEGIN.");

    m_gpuDevice =
        SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV
                | SDL_GPU_SHADERFORMAT_DXIL
                | SDL_GPU_SHADERFORMAT_MSL,
            true,
            nullptr);

    rendererDiagnosticLog(
        "SDL_CreateGPUDevice END device=%p error=%s",
        static_cast<void *>(
            m_gpuDevice),
        SDL_GetError());

    if (m_gpuDevice == nullptr)
    {
        m_error =
            SDL_GetError();

        rendererDiagnosticLog(
            "initialize FAIL: GPU device NULL.");

        shutdown();

        return false;
    }

    rendererDiagnosticLog(
        "SDL_ClaimWindowForGPUDevice BEGIN.");

    const bool claimResult =
        SDL_ClaimWindowForGPUDevice(
            m_gpuDevice,
            static_cast<SDL_Window *>(
                m_window));

    rendererDiagnosticLog(
        "SDL_ClaimWindowForGPUDevice END result=%d error=%s",
        claimResult ? 1 : 0,
        SDL_GetError());

    if (!claimResult)
    {
        m_error =
            SDL_GetError();

        shutdown();

        return false;
    }

    /*
     * Geometry buffers.
     */

    const SDL_GPUBufferCreateInfo
        vertexBufferInfo =
    {
        SDL_GPU_BUFFERUSAGE_VERTEX,

        static_cast<Uint32>(
            sizeof(EngineSimVertex)
            * MaxVertices),

        0
    };

    const SDL_GPUBufferCreateInfo
        indexBufferInfo =
    {
        SDL_GPU_BUFFERUSAGE_INDEX,

        static_cast<Uint32>(
            sizeof(std::uint16_t)
            * MaxIndices),

        0
    };

    const SDL_GPUTransferBufferCreateInfo
        vertexTransferInfo =
    {
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,

        static_cast<Uint32>(
            sizeof(EngineSimVertex)
            * MaxVertices),

        0
    };

    const SDL_GPUTransferBufferCreateInfo
        indexTransferInfo =
    {
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,

        static_cast<Uint32>(
            sizeof(std::uint16_t)
            * MaxIndices),

        0
    };

    rendererDiagnosticLog(
        "Creating GPU vertex/index buffers.");

    m_vertexBuffer =
        SDL_CreateGPUBuffer(
            m_gpuDevice,
            &vertexBufferInfo);

    rendererDiagnosticLog(
        "vertexBuffer=%p error=%s",
        static_cast<void *>(
            m_vertexBuffer),
        SDL_GetError());

    m_indexBuffer =
        SDL_CreateGPUBuffer(
            m_gpuDevice,
            &indexBufferInfo);

    rendererDiagnosticLog(
        "indexBuffer=%p error=%s",
        static_cast<void *>(
            m_indexBuffer),
        SDL_GetError());

    m_vertexTransferBuffer =
        SDL_CreateGPUTransferBuffer(
            m_gpuDevice,
            &vertexTransferInfo);

    rendererDiagnosticLog(
        "vertexTransferBuffer=%p error=%s",
        static_cast<void *>(
            m_vertexTransferBuffer),
        SDL_GetError());

    m_indexTransferBuffer =
        SDL_CreateGPUTransferBuffer(
            m_gpuDevice,
            &indexTransferInfo);

    rendererDiagnosticLog(
        "indexTransferBuffer=%p error=%s",
        static_cast<void *>(
            m_indexTransferBuffer),
        SDL_GetError());

    if (
        m_vertexBuffer == nullptr
        || m_indexBuffer == nullptr
        || m_vertexTransferBuffer == nullptr
        || m_indexTransferBuffer == nullptr)
    {
        m_error =
            SDL_GetError();

        rendererDiagnosticLog(
            "initialize FAIL: geometry buffer creation.");

        shutdown();

        return false;
    }

    /*
     * Load shaders.
     */

    const char *extension =
        nullptr;

    const SDL_GPUShaderFormat shaderFormat =
        selectedShaderFormat(
            m_gpuDevice,
            &extension);

    rendererDiagnosticLog(
        "Selected shader format=%u extension=%s",
        static_cast<unsigned>(
            shaderFormat),
        extension != nullptr
            ? extension
            : "(null)");

    if (
        shaderFormat
        == SDL_GPU_SHADERFORMAT_INVALID)
    {
        m_error =
            SDL_GetError();

        rendererDiagnosticLog(
            "initialize FAIL: no shader format.");

        shutdown();

        return false;
    }

    const std::string vertexPath =
        shaderDirectory
        + "/engine_sim.vertex."
        + extension;

    const std::string fragmentPath =
        shaderDirectory
        + "/engine_sim.fragment."
        + extension;

    rendererDiagnosticLog(
        "Loading vertex shader: %s",
        vertexPath.c_str());

    const std::vector<std::uint8_t>
        vertexCode =
            loadBinaryFile(
                vertexPath);

    rendererDiagnosticLog(
        "Vertex shader bytes=%llu",
        static_cast<unsigned long long>(
            vertexCode.size()));

    rendererDiagnosticLog(
        "Loading fragment shader: %s",
        fragmentPath.c_str());

    const std::vector<std::uint8_t>
        fragmentCode =
            loadBinaryFile(
                fragmentPath);

    rendererDiagnosticLog(
        "Fragment shader bytes=%llu",
        static_cast<unsigned long long>(
            fragmentCode.size()));

    if (
        vertexCode.empty()
        || fragmentCode.empty())
    {
        m_error =
            "Missing SDL GPU shader artifacts in "
            + shaderDirectory;

        rendererDiagnosticLog(
            "initialize FAIL: shader file empty.");

        shutdown();

        return false;
    }

    const char *vertexEntrypoint =
        "VSMain";

    const char *fragmentEntrypoint =
        "PSMain";

    const SDL_GPUShaderCreateInfo
        vertexInfo =
    {
        vertexCode.size(),
        vertexCode.data(),
        vertexEntrypoint,
        shaderFormat,
        SDL_GPU_SHADERSTAGE_VERTEX,

        0,
        0,
        0,
        1,
        0
    };

    const SDL_GPUShaderCreateInfo
        fragmentInfo =
    {
        fragmentCode.size(),
        fragmentCode.data(),
        fragmentEntrypoint,
        shaderFormat,
        SDL_GPU_SHADERSTAGE_FRAGMENT,

        0,
        0,
        0,
        1,
        0
    };

    rendererDiagnosticLog(
        "SDL_CreateGPUShader vertex BEGIN.");

    SDL_GPUShader *vertexShader =
        SDL_CreateGPUShader(
            m_gpuDevice,
            &vertexInfo);

    rendererDiagnosticLog(
        "SDL_CreateGPUShader vertex END shader=%p error=%s",
        static_cast<void *>(
            vertexShader),
        SDL_GetError());

    rendererDiagnosticLog(
        "SDL_CreateGPUShader fragment BEGIN.");

    SDL_GPUShader *fragmentShader =
        SDL_CreateGPUShader(
            m_gpuDevice,
            &fragmentInfo);

    rendererDiagnosticLog(
        "SDL_CreateGPUShader fragment END shader=%p error=%s",
        static_cast<void *>(
            fragmentShader),
        SDL_GetError());

    if (
        vertexShader == nullptr
        || fragmentShader == nullptr)
    {
        m_error =
            SDL_GetError();

        if (vertexShader != nullptr)
        {
            SDL_ReleaseGPUShader(
                m_gpuDevice,
                vertexShader);
        }

        if (fragmentShader != nullptr)
        {
            SDL_ReleaseGPUShader(
                m_gpuDevice,
                fragmentShader);
        }

        rendererDiagnosticLog(
            "initialize FAIL: shader creation.");

        shutdown();

        return false;
    }

    /*
     * Vertex layout.
     */

    const SDL_GPUVertexBufferDescription
        vertexBufferDescription =
    {
        0,

        static_cast<Uint32>(
            sizeof(EngineSimVertex)),

        SDL_GPU_VERTEXINPUTRATE_VERTEX,

        0
    };

    const SDL_GPUVertexAttribute
        vertexAttributes[] =
    {
        {
            0,
            0,
            SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,

            static_cast<Uint32>(
                offsetof(
                    EngineSimVertex,
                    Pos))
        },

        {
            1,
            0,
            SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,

            static_cast<Uint32>(
                offsetof(
                    EngineSimVertex,
                    Normal))
        },

        {
            2,
            0,
            SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,

            static_cast<Uint32>(
                offsetof(
                    EngineSimVertex,
                    TexCoord))
        }
    };

    SDL_GPUColorTargetDescription
        colorTarget = {};

    colorTarget.format =
        SDL_GetGPUSwapchainTextureFormat(
            m_gpuDevice,
            static_cast<SDL_Window *>(
                m_window));

    rendererDiagnosticLog(
        "Swapchain format=%u",
        static_cast<unsigned>(
            colorTarget.format));

    colorTarget.blend_state.enable_blend =
        true;

    colorTarget.blend_state.src_color_blendfactor =
        SDL_GPU_BLENDFACTOR_SRC_ALPHA;

    colorTarget.blend_state.dst_color_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    colorTarget.blend_state.color_blend_op =
        SDL_GPU_BLENDOP_ADD;

    colorTarget.blend_state.src_alpha_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE;

    colorTarget.blend_state.dst_alpha_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    colorTarget.blend_state.alpha_blend_op =
        SDL_GPU_BLENDOP_ADD;

    SDL_GPUGraphicsPipelineCreateInfo
        pipelineInfo = {};

    pipelineInfo.vertex_shader =
        vertexShader;

    pipelineInfo.fragment_shader =
        fragmentShader;

    pipelineInfo.vertex_input_state =
    {
        &vertexBufferDescription,
        1,

        vertexAttributes,
        3
    };

    pipelineInfo.primitive_type =
        SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

#if defined(ENGINE_SIM_IOS)

    pipelineInfo.target_info.color_target_descriptions =
        &colorTarget;

    pipelineInfo.target_info.num_color_targets =
        1;

    pipelineInfo.target_info.depth_stencil_format =
        SDL_GPU_TEXTUREFORMAT_INVALID;

    pipelineInfo.target_info.has_depth_stencil_target =
        false;

    pipelineInfo.depth_stencil_state = {};

#else

    pipelineInfo.target_info.color_target_descriptions =
        &colorTarget;

    pipelineInfo.target_info.num_color_targets =
        1;

    pipelineInfo.target_info.depth_stencil_format =
        SDL_GPU_TEXTUREFORMAT_D16_UNORM;

    pipelineInfo.target_info.has_depth_stencil_target =
        true;

    pipelineInfo.depth_stencil_state.enable_depth_test =
        true;

    pipelineInfo.depth_stencil_state.enable_depth_write =
        true;

    pipelineInfo.depth_stencil_state.compare_op =
        SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

#endif

    rendererDiagnosticLog(
        "SDL_CreateGPUGraphicsPipeline scene BEGIN.");

    m_scenePipeline =
        SDL_CreateGPUGraphicsPipeline(
            m_gpuDevice,
            &pipelineInfo);

    rendererDiagnosticLog(
        "SDL_CreateGPUGraphicsPipeline scene END pipeline=%p error=%s",
        static_cast<void *>(
            m_scenePipeline),
        SDL_GetError());

    pipelineInfo.target_info.depth_stencil_format =
        SDL_GPU_TEXTUREFORMAT_INVALID;

    pipelineInfo.target_info.has_depth_stencil_target =
        false;

    pipelineInfo.depth_stencil_state = {};

    rendererDiagnosticLog(
        "SDL_CreateGPUGraphicsPipeline UI BEGIN.");

    m_uiPipeline =
        SDL_CreateGPUGraphicsPipeline(
            m_gpuDevice,
            &pipelineInfo);

    rendererDiagnosticLog(
        "SDL_CreateGPUGraphicsPipeline UI END pipeline=%p error=%s",
        static_cast<void *>(
            m_uiPipeline),
        SDL_GetError());

    if (
        m_scenePipeline == nullptr
        || m_uiPipeline == nullptr)
    {
        m_error =
            SDL_GetError();
    }

    SDL_ReleaseGPUShader(
        m_gpuDevice,
        vertexShader);

    SDL_ReleaseGPUShader(
        m_gpuDevice,
        fragmentShader);

    if (
        m_scenePipeline == nullptr
        || m_uiPipeline == nullptr)
    {
        if (m_error.empty())
        {
            m_error =
                "SDL GPU graphics pipeline creation failed";
        }

        rendererDiagnosticLog(
            "initialize FAIL: pipeline creation.");

        shutdown();

        return false;
    }

    SDL_Log(
        "EngineSim: SDL GPU pipelines created successfully.");

#if defined(ENGINE_SIM_IOS)

    SDL_Log(
        "EngineSim: iOS depth path disabled for Metal bring-up.");

#endif

    rendererDiagnosticLog(
        "initialize SUCCESS.");

    return true;
}

void SdlGpuRenderer::shutdown()
{
    rendererDiagnosticLog(
        "shutdown ENTER device=%p window=%p",
        static_cast<void *>(
            m_gpuDevice),
        m_window);

    if (m_gpuDevice != nullptr)
    {
        if (m_scenePipeline != nullptr)
        {
            SDL_ReleaseGPUGraphicsPipeline(
                m_gpuDevice,
                m_scenePipeline);
        }

        if (m_uiPipeline != nullptr)
        {
            SDL_ReleaseGPUGraphicsPipeline(
                m_gpuDevice,
                m_uiPipeline);
        }

        if (m_sceneTexture != nullptr)
        {
            SDL_ReleaseGPUTexture(
                m_gpuDevice,
                m_sceneTexture);
        }

        if (m_depthTexture != nullptr)
        {
            SDL_ReleaseGPUTexture(
                m_gpuDevice,
                m_depthTexture);
        }

        if (m_vertexTransferBuffer != nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(
                m_gpuDevice,
                m_vertexTransferBuffer);
        }

        if (m_indexTransferBuffer != nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(
                m_gpuDevice,
                m_indexTransferBuffer);
        }

        if (m_vertexBuffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(
                m_gpuDevice,
                m_vertexBuffer);
        }

        if (m_indexBuffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(
                m_gpuDevice,
                m_indexBuffer);
        }

        m_vertexTransferBuffer =
            nullptr;

        m_indexTransferBuffer =
            nullptr;

        m_vertexBuffer =
            nullptr;

        m_indexBuffer =
            nullptr;

        m_scenePipeline =
            nullptr;

        m_uiPipeline =
            nullptr;

        m_sceneTexture =
            nullptr;

        m_depthTexture =
            nullptr;

        m_sceneTextureWidth =
            0;

        m_sceneTextureHeight =
            0;

        m_depthTextureWidth =
            0;

        m_depthTextureHeight =
            0;

        if (m_window != nullptr)
        {
            SDL_ReleaseWindowFromGPUDevice(
                m_gpuDevice,
                static_cast<SDL_Window *>(
                    m_window));
        }

        SDL_DestroyGPUDevice(
            m_gpuDevice);

        m_gpuDevice =
            nullptr;
    }

    m_window =
        nullptr;

    rendererDiagnosticLog(
        "shutdown END.");
}

void SdlGpuRenderer::beginFrame(
    const ysVector &clearColor)
{
    m_clearColor =
        clearColor;

    m_vertices =
        nullptr;

    m_indices =
        nullptr;

    m_vertexCount =
        0;

    m_indexCount =
        0;

    m_submissions.clear();
}

void SdlGpuRenderer::setSceneViewport(
    float x,
    float y,
    float width,
    float height)
{
    m_sceneViewportX =
        x;

    m_sceneViewportY =
        y;

    m_sceneViewportWidth =
        std::max(
            0.0f,
            width);

    m_sceneViewportHeight =
        std::max(
            0.0f,
            height);
}

void SdlGpuRenderer::uploadGeometry(
    const EngineSimVertex *vertices,
    int vertexCount,
    const std::uint16_t *indices,
    int indexCount)
{
    if (
        vertexCount < 0
        || vertexCount > MaxVertices
        || indexCount < 0
        || indexCount > MaxIndices)
    {
        rendererDiagnosticLog(
            "uploadGeometry REJECTED vertices=%d indices=%d",
            vertexCount,
            indexCount);

        return;
    }

    m_vertices =
        vertices;

    m_indices =
        indices;

    m_vertexCount =
        vertexCount;

    m_indexCount =
        indexCount;
}

void SdlGpuRenderer::submitGeometry(
    const EngineSimVertex *,
    const std::uint16_t *,
    int baseVertex,
    int baseIndex,
    int faceCount,
    const ysMatrix &transform,
    const ysMatrix &cameraView,
    const ysMatrix &projection,
    const ysVector &color,
    std::uint32_t stage,
    int layer)
{
    m_submissions.push_back(
    {
        baseVertex,
        baseIndex,
        faceCount,

        transform,
        cameraView,
        projection,

        color,

        stage,
        layer
    });
}

void SdlGpuRenderer::endFrame()
{
    static std::uint64_t frameNumber =
        0;

    ++frameNumber;

    const bool verboseFrame =
        frameNumber <= 20
        || (frameNumber % 120) == 0;

    if (verboseFrame)
    {
        rendererDiagnosticLog(
            "============================================================");

        rendererDiagnosticLog(
            "GPU FRAME %llu ENTER",
            static_cast<unsigned long long>(
                frameNumber));

        rendererDiagnosticLog(
            "state device=%p window=%p vertices=%d indices=%d submissions=%llu",
            static_cast<void *>(
                m_gpuDevice),
            m_window,
            m_vertexCount,
            m_indexCount,
            static_cast<unsigned long long>(
                m_submissions.size()));

        rendererDiagnosticLog(
            "scene viewport x=%.1f y=%.1f w=%.1f h=%.1f",
            static_cast<double>(
                m_sceneViewportX),
            static_cast<double>(
                m_sceneViewportY),
            static_cast<double>(
                m_sceneViewportWidth),
            static_cast<double>(
                m_sceneViewportHeight));
    }

    if (
        m_gpuDevice == nullptr
        || m_window == nullptr)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu ABORT: null device/window.",
            static_cast<unsigned long long>(
                frameNumber));

        return;
    }

    if (verboseFrame)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_AcquireGPUCommandBuffer BEGIN",
            static_cast<unsigned long long>(
                frameNumber));
    }

    SDL_GPUCommandBuffer *commands =
        SDL_AcquireGPUCommandBuffer(
            m_gpuDevice);

    if (verboseFrame)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_AcquireGPUCommandBuffer END commands=%p error=%s",
            static_cast<unsigned long long>(
                frameNumber),
            static_cast<void *>(
                commands),
            SDL_GetError());
    }

    if (commands == nullptr)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu ABORT: command buffer NULL.",
            static_cast<unsigned long long>(
                frameNumber));

        return;
    }

    /*
     * ========================================================
     * Upload generated EngineSim geometry
     * ========================================================
     */

    if (
        m_vertexCount > 0
        && m_indexCount > 0)
    {
        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: map vertex transfer BEGIN",
                static_cast<unsigned long long>(
                    frameNumber));
        }

        void *vertexUpload =
            SDL_MapGPUTransferBuffer(
                m_gpuDevice,
                m_vertexTransferBuffer,
                true);

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: map vertex transfer END ptr=%p error=%s",
                static_cast<unsigned long long>(
                    frameNumber),
                vertexUpload,
                SDL_GetError());

            rendererDiagnosticLog(
                "GPU FRAME %llu: map index transfer BEGIN",
                static_cast<unsigned long long>(
                    frameNumber));
        }

        void *indexUpload =
            SDL_MapGPUTransferBuffer(
                m_gpuDevice,
                m_indexTransferBuffer,
                true);

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: map index transfer END ptr=%p error=%s",
                static_cast<unsigned long long>(
                    frameNumber),
                indexUpload,
                SDL_GetError());
        }

        if (
            vertexUpload != nullptr
            && indexUpload != nullptr)
        {
            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: memcpy geometry BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber));
            }

            std::memcpy(
                vertexUpload,
                m_vertices,

                sizeof(EngineSimVertex)
                * static_cast<std::size_t>(
                    m_vertexCount));

            std::memcpy(
                indexUpload,
                m_indices,

                sizeof(std::uint16_t)
                * static_cast<std::size_t>(
                    m_indexCount));

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: memcpy geometry END",
                    static_cast<unsigned long long>(
                        frameNumber));
            }
        }

        if (vertexUpload != nullptr)
        {
            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: unmap vertex BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber));
            }

            SDL_UnmapGPUTransferBuffer(
                m_gpuDevice,
                m_vertexTransferBuffer);

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: unmap vertex END",
                    static_cast<unsigned long long>(
                        frameNumber));
            }
        }

        if (indexUpload != nullptr)
        {
            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: unmap index BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber));
            }

            SDL_UnmapGPUTransferBuffer(
                m_gpuDevice,
                m_indexTransferBuffer);

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: unmap index END",
                    static_cast<unsigned long long>(
                        frameNumber));
            }
        }

        if (
            vertexUpload != nullptr
            && indexUpload != nullptr)
        {
            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: SDL_BeginGPUCopyPass BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber));
            }

            SDL_GPUCopyPass *copyPass =
                SDL_BeginGPUCopyPass(
                    commands);

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: SDL_BeginGPUCopyPass END pass=%p error=%s",
                    static_cast<unsigned long long>(
                        frameNumber),
                    static_cast<void *>(
                        copyPass),
                    SDL_GetError());
            }

            if (copyPass != nullptr)
            {
                const SDL_GPUTransferBufferLocation
                    vertexSource =
                {
                    m_vertexTransferBuffer,
                    0
                };

                const SDL_GPUTransferBufferLocation
                    indexSource =
                {
                    m_indexTransferBuffer,
                    0
                };

                const SDL_GPUBufferRegion
                    vertexDestination =
                {
                    m_vertexBuffer,
                    0,

                    static_cast<Uint32>(
                        sizeof(EngineSimVertex)
                        * m_vertexCount)
                };

                const SDL_GPUBufferRegion
                    indexDestination =
                {
                    m_indexBuffer,
                    0,

                    static_cast<Uint32>(
                        sizeof(std::uint16_t)
                        * m_indexCount)
                };

                if (verboseFrame)
                {
                    rendererDiagnosticLog(
                        "GPU FRAME %llu: SDL_UploadToGPUBuffer VERTEX BEGIN",
                        static_cast<unsigned long long>(
                            frameNumber));
                }

                SDL_UploadToGPUBuffer(
                    copyPass,
                    &vertexSource,
                    &vertexDestination,
                    true);

                if (verboseFrame)
                {
                    rendererDiagnosticLog(
                        "GPU FRAME %llu: SDL_UploadToGPUBuffer VERTEX END",
                        static_cast<unsigned long long>(
                            frameNumber));

                    rendererDiagnosticLog(
                        "GPU FRAME %llu: SDL_UploadToGPUBuffer INDEX BEGIN",
                        static_cast<unsigned long long>(
                            frameNumber));
                }

                SDL_UploadToGPUBuffer(
                    copyPass,
                    &indexSource,
                    &indexDestination,
                    true);

                if (verboseFrame)
                {
                    rendererDiagnosticLog(
                        "GPU FRAME %llu: SDL_UploadToGPUBuffer INDEX END",
                        static_cast<unsigned long long>(
                            frameNumber));

                    rendererDiagnosticLog(
                        "GPU FRAME %llu: SDL_EndGPUCopyPass BEGIN",
                        static_cast<unsigned long long>(
                            frameNumber));
                }

                SDL_EndGPUCopyPass(
                    copyPass);

                if (verboseFrame)
                {
                    rendererDiagnosticLog(
                        "GPU FRAME %llu: SDL_EndGPUCopyPass END",
                        static_cast<unsigned long long>(
                            frameNumber));
                }
            }
        }
    }
    else if (verboseFrame)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: geometry upload skipped.",
            static_cast<unsigned long long>(
                frameNumber));
    }

    /*
     * ========================================================
     * Acquire Metal swapchain texture
     * ========================================================
     */

    SDL_GPUTexture *swapchainTexture =
        nullptr;

    Uint32 swapchainWidth =
        0;

    Uint32 swapchainHeight =
        0;

    if (verboseFrame)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_WaitAndAcquireGPUSwapchainTexture BEGIN",
            static_cast<unsigned long long>(
                frameNumber));
    }

    const bool acquired =
        SDL_WaitAndAcquireGPUSwapchainTexture(
            commands,
            static_cast<SDL_Window *>(
                m_window),
            &swapchainTexture,
            &swapchainWidth,
            &swapchainHeight);

    if (verboseFrame)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_WaitAndAcquireGPUSwapchainTexture END result=%d texture=%p size=%ux%u error=%s",
            static_cast<unsigned long long>(
                frameNumber),
            acquired ? 1 : 0,
            static_cast<void *>(
                swapchainTexture),
            swapchainWidth,
            swapchainHeight,
            SDL_GetError());
    }

    if (!acquired)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_CancelGPUCommandBuffer BEGIN",
            static_cast<unsigned long long>(
                frameNumber));

        SDL_CancelGPUCommandBuffer(
            commands);

        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_CancelGPUCommandBuffer END",
            static_cast<unsigned long long>(
                frameNumber));

        return;
    }

    if (swapchainTexture == nullptr)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: swapchain texture NULL, submitting empty command buffer.",
            static_cast<unsigned long long>(
                frameNumber));

        SDL_SubmitGPUCommandBuffer(
            commands);

        rendererDiagnosticLog(
            "GPU FRAME %llu: empty command buffer submitted.",
            static_cast<unsigned long long>(
                frameNumber));

        return;
    }

    /*
     * Helper: draw one EngineSim render stage.
     */

    const auto drawStage =
        [&](SDL_GPURenderPass *pass,
            std::uint32_t stage)
    {
        std::vector<const Submission *>
            submissions;

        for (
            const Submission &submission
            : m_submissions)
        {
            if (
                (submission.stage & stage)
                != 0)
            {
                submissions.push_back(
                    &submission);
            }
        }

        std::stable_sort(
            submissions.begin(),
            submissions.end(),

            [](
                const Submission *left,
                const Submission *right)
            {
                return
                    left->layer
                    < right->layer;
            });

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: drawStage stage=0x%08x submissions=%llu",
                static_cast<unsigned long long>(
                    frameNumber),
                static_cast<unsigned>(
                    stage),
                static_cast<unsigned long long>(
                    submissions.size()));
        }

        std::size_t drawNumber =
            0;

        for (
            const Submission *submission
            : submissions)
        {
            ++drawNumber;

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: DRAW %llu/%llu BEGIN layer=%d baseVertex=%d baseIndex=%d faces=%d stage=0x%08x",
                    static_cast<unsigned long long>(
                        frameNumber),
                    static_cast<unsigned long long>(
                        drawNumber),
                    static_cast<unsigned long long>(
                        submissions.size()),
                    submission->layer,
                    submission->baseVertex,
                    submission->baseIndex,
                    submission->faceCount,
                    static_cast<unsigned>(
                        submission->stage));
            }

            const ysMatrix transforms[] =
            {
                submission->transform,
                submission->cameraView,
                submission->projection
            };

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: DRAW %llu vertex uniform BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber),
                    static_cast<unsigned long long>(
                        drawNumber));
            }

            SDL_PushGPUVertexUniformData(
                commands,
                0,
                transforms,
                sizeof(transforms));

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: DRAW %llu vertex uniform END",
                    static_cast<unsigned long long>(
                        frameNumber),
                    static_cast<unsigned long long>(
                        drawNumber));

                rendererDiagnosticLog(
                    "GPU FRAME %llu: DRAW %llu fragment uniform BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber),
                    static_cast<unsigned long long>(
                        drawNumber));
            }

            SDL_PushGPUFragmentUniformData(
                commands,
                0,
                &submission->color,
                sizeof(submission->color));

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: DRAW %llu fragment uniform END",
                    static_cast<unsigned long long>(
                        frameNumber),
                    static_cast<unsigned long long>(
                        drawNumber));

                rendererDiagnosticLog(
                    "GPU FRAME %llu: DRAW %llu SDL_DrawGPUIndexedPrimitives BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber),
                    static_cast<unsigned long long>(
                        drawNumber));
            }

            SDL_DrawGPUIndexedPrimitives(
                pass,

                static_cast<Uint32>(
                    submission->faceCount
                    * 3),

                1,

                static_cast<Uint32>(
                    submission->baseIndex),

                submission->baseVertex,

                0);

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: DRAW %llu SDL_DrawGPUIndexedPrimitives END",
                    static_cast<unsigned long long>(
                        frameNumber),
                    static_cast<unsigned long long>(
                        drawNumber));
            }
        }
    };

    const auto bindGeometry =
        [&](SDL_GPURenderPass *pass,
            SDL_GPUGraphicsPipeline *pipeline)
    {
        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: SDL_BindGPUGraphicsPipeline BEGIN pipeline=%p",
                static_cast<unsigned long long>(
                    frameNumber),
                static_cast<void *>(
                    pipeline));
        }

        SDL_BindGPUGraphicsPipeline(
            pass,
            pipeline);

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: SDL_BindGPUGraphicsPipeline END",
                static_cast<unsigned long long>(
                    frameNumber));
        }

        const SDL_GPUBufferBinding
            vertexBinding =
        {
            m_vertexBuffer,
            0
        };

        const SDL_GPUBufferBinding
            indexBinding =
        {
            m_indexBuffer,
            0
        };

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: SDL_BindGPUVertexBuffers BEGIN",
                static_cast<unsigned long long>(
                    frameNumber));
        }

        SDL_BindGPUVertexBuffers(
            pass,
            0,
            &vertexBinding,
            1);

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: SDL_BindGPUVertexBuffers END",
                static_cast<unsigned long long>(
                    frameNumber));

            rendererDiagnosticLog(
                "GPU FRAME %llu: SDL_BindGPUIndexBuffer BEGIN",
                static_cast<unsigned long long>(
                    frameNumber));
        }

        SDL_BindGPUIndexBuffer(
            pass,
            &indexBinding,
            SDL_GPU_INDEXELEMENTSIZE_16BIT);

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: SDL_BindGPUIndexBuffer END",
                static_cast<unsigned long long>(
                    frameNumber));
        }
    };

#if defined(ENGINE_SIM_IOS)

    /*
     * ========================================================
     * iOS Metal path
     * ========================================================
     */

    SDL_GPUColorTargetInfo
        colorTarget = {};

    colorTarget.texture =
        swapchainTexture;

    colorTarget.clear_color =
    {
        m_clearColor.x,
        m_clearColor.y,
        m_clearColor.z,
        m_clearColor.w
    };

    colorTarget.load_op =
        SDL_GPU_LOADOP_CLEAR;

    colorTarget.store_op =
        SDL_GPU_STOREOP_STORE;

    if (verboseFrame)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_BeginGPURenderPass BEGIN",
            static_cast<unsigned long long>(
                frameNumber));
    }

    SDL_GPURenderPass *pass =
        SDL_BeginGPURenderPass(
            commands,
            &colorTarget,
            1,
            nullptr);

    if (verboseFrame)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_BeginGPURenderPass END pass=%p error=%s",
            static_cast<unsigned long long>(
                frameNumber),
            static_cast<void *>(
                pass),
            SDL_GetError());
    }

    if (pass != nullptr)
    {
        /*
         * Mechanical scene.
         */

        if (
            m_scenePipeline != nullptr
            && m_vertexCount > 0
            && m_indexCount > 0)
        {
            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: SCENE bindGeometry BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber));
            }

            bindGeometry(
                pass,
                m_scenePipeline);

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: SCENE bindGeometry END",
                    static_cast<unsigned long long>(
                        frameNumber));
            }

            if (
                m_sceneViewportWidth > 0.0f
                && m_sceneViewportHeight > 0.0f)
            {
                SDL_GPUViewport viewport =
                {
                    m_sceneViewportX,
                    m_sceneViewportY,

                    m_sceneViewportWidth,
                    m_sceneViewportHeight,

                    0.0f,
                    1.0f
                };

                if (verboseFrame)
                {
                    rendererDiagnosticLog(
                        "GPU FRAME %llu: SCENE SDL_SetGPUViewport BEGIN",
                        static_cast<unsigned long long>(
                            frameNumber));
                }

                SDL_SetGPUViewport(
                    pass,
                    &viewport);

                if (verboseFrame)
                {
                    rendererDiagnosticLog(
                        "GPU FRAME %llu: SCENE SDL_SetGPUViewport END",
                        static_cast<unsigned long long>(
                            frameNumber));
                }
            }

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: SCENE drawStage BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber));
            }

            drawStage(
                pass,
                Shaders::SceneStage);

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: SCENE drawStage END",
                    static_cast<unsigned long long>(
                        frameNumber));
            }
        }
        else if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: SCENE skipped pipeline=%p vertices=%d indices=%d",
                static_cast<unsigned long long>(
                    frameNumber),
                static_cast<void *>(
                    m_scenePipeline),
                m_vertexCount,
                m_indexCount);
        }

        /*
         * Restore full-screen viewport before UI.
         */

        SDL_GPUViewport fullViewport =
        {
            0.0f,
            0.0f,

            static_cast<float>(
                swapchainWidth),

            static_cast<float>(
                swapchainHeight),

            0.0f,
            1.0f
        };

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: FULL SDL_SetGPUViewport BEGIN",
                static_cast<unsigned long long>(
                    frameNumber));
        }

        SDL_SetGPUViewport(
            pass,
            &fullViewport);

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: FULL SDL_SetGPUViewport END",
                static_cast<unsigned long long>(
                    frameNumber));
        }

        /*
         * UI.
         */

        if (
            m_uiPipeline != nullptr
            && m_vertexCount > 0
            && m_indexCount > 0)
        {
            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: UI bindGeometry BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber));
            }

            bindGeometry(
                pass,
                m_uiPipeline);

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: UI bindGeometry END",
                    static_cast<unsigned long long>(
                        frameNumber));

                rendererDiagnosticLog(
                    "GPU FRAME %llu: UI drawStage BEGIN",
                    static_cast<unsigned long long>(
                        frameNumber));
            }

            drawStage(
                pass,
                Shaders::UiStage);

            if (verboseFrame)
            {
                rendererDiagnosticLog(
                    "GPU FRAME %llu: UI drawStage END",
                    static_cast<unsigned long long>(
                        frameNumber));
            }
        }
        else if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: UI skipped pipeline=%p vertices=%d indices=%d",
                static_cast<unsigned long long>(
                    frameNumber),
                static_cast<void *>(
                    m_uiPipeline),
                m_vertexCount,
                m_indexCount);
        }

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: SDL_EndGPURenderPass BEGIN",
                static_cast<unsigned long long>(
                    frameNumber));
        }

        SDL_EndGPURenderPass(
            pass);

        if (verboseFrame)
        {
            rendererDiagnosticLog(
                "GPU FRAME %llu: SDL_EndGPURenderPass END",
                static_cast<unsigned long long>(
                    frameNumber));
        }
    }
    else
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: render pass NULL.",
            static_cast<unsigned long long>(
                frameNumber));
    }

#else

    /*
     * ========================================================
     * Desktop path
     * ========================================================
     */

    const Uint32 sceneWidth =
        std::max(
            1u,
            static_cast<Uint32>(
                m_sceneViewportWidth));

    const Uint32 sceneHeight =
        std::max(
            1u,
            static_cast<Uint32>(
                m_sceneViewportHeight));

    if (
        m_sceneTexture == nullptr
        || m_sceneTextureWidth != sceneWidth
        || m_sceneTextureHeight != sceneHeight)
    {
        if (m_sceneTexture != nullptr)
        {
            SDL_ReleaseGPUTexture(
                m_gpuDevice,
                m_sceneTexture);
        }

        const SDL_GPUTextureCreateInfo
            sceneInfo =
        {
            SDL_GPU_TEXTURETYPE_2D,

            SDL_GetGPUSwapchainTextureFormat(
                m_gpuDevice,
                static_cast<SDL_Window *>(
                    m_window)),

            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
                | SDL_GPU_TEXTUREUSAGE_SAMPLER,

            sceneWidth,
            sceneHeight,

            1,
            1,

            SDL_GPU_SAMPLECOUNT_1,

            0
        };

        m_sceneTexture =
            SDL_CreateGPUTexture(
                m_gpuDevice,
                &sceneInfo);

        m_sceneTextureWidth =
            sceneWidth;

        m_sceneTextureHeight =
            sceneHeight;
    }

    if (
        m_depthTexture == nullptr
        || m_depthTextureWidth != sceneWidth
        || m_depthTextureHeight != sceneHeight)
    {
        if (m_depthTexture != nullptr)
        {
            SDL_ReleaseGPUTexture(
                m_gpuDevice,
                m_depthTexture);
        }

        const SDL_GPUTextureCreateInfo
            depthInfo =
        {
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,

            sceneWidth,
            sceneHeight,

            1,
            1,

            SDL_GPU_SAMPLECOUNT_1,

            0
        };

        m_depthTexture =
            SDL_CreateGPUTexture(
                m_gpuDevice,
                &depthInfo);

        m_depthTextureWidth =
            sceneWidth;

        m_depthTextureHeight =
            sceneHeight;
    }

    SDL_GPUColorTargetInfo
        sceneColorTarget = {};

    sceneColorTarget.texture =
        m_sceneTexture;

    sceneColorTarget.clear_color =
    {
        m_clearColor.x,
        m_clearColor.y,
        m_clearColor.z,
        m_clearColor.w
    };

    sceneColorTarget.load_op =
        SDL_GPU_LOADOP_CLEAR;

    sceneColorTarget.store_op =
        SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo
        depthTarget = {};

    depthTarget.texture =
        m_depthTexture;

    depthTarget.clear_depth =
        1.0f;

    depthTarget.load_op =
        SDL_GPU_LOADOP_CLEAR;

    depthTarget.store_op =
        SDL_GPU_STOREOP_DONT_CARE;

    depthTarget.stencil_load_op =
        SDL_GPU_LOADOP_DONT_CARE;

    depthTarget.stencil_store_op =
        SDL_GPU_STOREOP_DONT_CARE;

    SDL_GPUColorTargetInfo
        clearTarget =
            sceneColorTarget;

    clearTarget.texture =
        swapchainTexture;

    SDL_GPURenderPass *clearPass =
        SDL_BeginGPURenderPass(
            commands,
            &clearTarget,
            1,
            nullptr);

    if (clearPass != nullptr)
    {
        SDL_EndGPURenderPass(
            clearPass);
    }

    SDL_GPURenderPass *scenePass =
        SDL_BeginGPURenderPass(
            commands,
            &sceneColorTarget,
            1,
            m_depthTexture != nullptr
                ? &depthTarget
                : nullptr);

    if (
        scenePass != nullptr
        && m_scenePipeline != nullptr
        && m_vertexCount > 0
        && m_indexCount > 0)
    {
        bindGeometry(
            scenePass,
            m_scenePipeline);

        if (
            m_sceneViewportWidth > 0.0f
            && m_sceneViewportHeight > 0.0f)
        {
            const SDL_GPUViewport
                sceneViewport =
            {
                0.0f,
                0.0f,

                static_cast<float>(
                    sceneWidth),

                static_cast<float>(
                    sceneHeight),

                0.0f,
                1.0f
            };

            SDL_SetGPUViewport(
                scenePass,
                &sceneViewport);
        }

        drawStage(
            scenePass,
            Shaders::SceneStage);
    }

    if (scenePass != nullptr)
    {
        SDL_EndGPURenderPass(
            scenePass);
    }

    if (m_sceneTexture != nullptr)
    {
        const SDL_GPUBlitInfo
            blitInfo =
        {
            {
                m_sceneTexture,

                0,
                0,

                0,
                0,

                sceneWidth,
                sceneHeight
            },

            {
                swapchainTexture,

                0,
                0,

                static_cast<Uint32>(
                    m_sceneViewportX),

                static_cast<Uint32>(
                    m_sceneViewportY),

                sceneWidth,
                sceneHeight
            },

            SDL_GPU_LOADOP_LOAD,

            {},

            SDL_FLIP_NONE,

            SDL_GPU_FILTER_LINEAR,

            false,

            0,
            0,
            0
        };

        SDL_BlitGPUTexture(
            commands,
            &blitInfo);
    }

    SDL_GPUColorTargetInfo
        uiColorTarget =
            sceneColorTarget;

    uiColorTarget.texture =
        swapchainTexture;

    uiColorTarget.load_op =
        SDL_GPU_LOADOP_LOAD;

    SDL_GPURenderPass *uiPass =
        SDL_BeginGPURenderPass(
            commands,
            &uiColorTarget,
            1,
            nullptr);

    if (
        uiPass != nullptr
        && m_uiPipeline != nullptr
        && m_vertexCount > 0
        && m_indexCount > 0)
    {
        bindGeometry(
            uiPass,
            m_uiPipeline);

        drawStage(
            uiPass,
            Shaders::UiStage);
    }

    if (uiPass != nullptr)
    {
        SDL_EndGPURenderPass(
            uiPass);
    }

#endif

    if (verboseFrame)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_SubmitGPUCommandBuffer BEGIN",
            static_cast<unsigned long long>(
                frameNumber));
    }

    const bool submitResult =
        SDL_SubmitGPUCommandBuffer(
            commands);

    if (verboseFrame)
    {
        rendererDiagnosticLog(
            "GPU FRAME %llu: SDL_SubmitGPUCommandBuffer END result=%d error=%s",
            static_cast<unsigned long long>(
                frameNumber),
            submitResult ? 1 : 0,
            SDL_GetError());

        rendererDiagnosticLog(
            "GPU FRAME %llu END",
            static_cast<unsigned long long>(
                frameNumber));

        rendererDiagnosticLog(
            "============================================================");
    }
}

const char *SdlGpuRenderer::lastError() const
{
    return
        m_error.empty()
            ? SDL_GetError()
            : m_error.c_str();
}
