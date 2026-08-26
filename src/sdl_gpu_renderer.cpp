#include "../include/sdl_gpu_renderer.h"
#include "../include/shaders.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr int MaxVertices = 500000;
constexpr int MaxIndices = 1000000;

std::vector<std::uint8_t> loadBinaryFile(
    const std::string &path)
{
    std::ifstream file(
        path,
        std::ios::binary | std::ios::ate);

    if (!file)
        return {};

    const std::streamsize size =
        file.tellg();

    if (size <= 0)
        return {};

    std::vector<std::uint8_t> data(
        static_cast<std::size_t>(size));

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
        SDL_GetGPUShaderFormats(device);

#if defined(__APPLE__)

    if (
        (formats & SDL_GPU_SHADERFORMAT_MSL)
        != 0)
    {
        *extension = "msl";

        return SDL_GPU_SHADERFORMAT_MSL;
    }

#endif

    if (
        (formats & SDL_GPU_SHADERFORMAT_SPIRV)
        != 0)
    {
        *extension = "spv";

        return SDL_GPU_SHADERFORMAT_SPIRV;
    }

    if (
        (formats & SDL_GPU_SHADERFORMAT_DXIL)
        != 0)
    {
        *extension = "dxil";

        return SDL_GPU_SHADERFORMAT_DXIL;
    }

    if (
        (formats & SDL_GPU_SHADERFORMAT_MSL)
        != 0)
    {
        *extension = "msl";

        return SDL_GPU_SHADERFORMAT_MSL;
    }

    *extension = nullptr;

    return SDL_GPU_SHADERFORMAT_INVALID;
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
}

SdlGpuRenderer::~SdlGpuRenderer()
{
    shutdown();
}

bool SdlGpuRenderer::initialize(
    void *nativeWindowHandle,
    const std::string &shaderDirectory)
{
    m_error.clear();

    m_window =
        nativeWindowHandle;

    if (m_window == nullptr)
    {
        m_error =
            "SDL window handle is null";

        return false;
    }

    /*
     * Prefer Metal Shader Language on Apple.
     */

    m_gpuDevice =
        SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV
                | SDL_GPU_SHADERFORMAT_DXIL
                | SDL_GPU_SHADERFORMAT_MSL,
            true,
            nullptr);

    if (
        m_gpuDevice == nullptr
        || !SDL_ClaimWindowForGPUDevice(
            m_gpuDevice,
            static_cast<SDL_Window *>(
                m_window)))
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

    m_vertexBuffer =
        SDL_CreateGPUBuffer(
            m_gpuDevice,
            &vertexBufferInfo);

    m_indexBuffer =
        SDL_CreateGPUBuffer(
            m_gpuDevice,
            &indexBufferInfo);

    m_vertexTransferBuffer =
        SDL_CreateGPUTransferBuffer(
            m_gpuDevice,
            &vertexTransferInfo);

    m_indexTransferBuffer =
        SDL_CreateGPUTransferBuffer(
            m_gpuDevice,
            &indexTransferInfo);

    if (
        m_vertexBuffer == nullptr
        || m_indexBuffer == nullptr
        || m_vertexTransferBuffer == nullptr
        || m_indexTransferBuffer == nullptr)
    {
        m_error =
            SDL_GetError();

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

    if (
        shaderFormat
        == SDL_GPU_SHADERFORMAT_INVALID)
    {
        m_error =
            SDL_GetError();

        shutdown();

        return false;
    }

    const std::vector<std::uint8_t>
        vertexCode =
            loadBinaryFile(
                shaderDirectory
                + "/engine_sim.vertex."
                + extension);

    const std::vector<std::uint8_t>
        fragmentCode =
            loadBinaryFile(
                shaderDirectory
                + "/engine_sim.fragment."
                + extension);

    if (
        vertexCode.empty()
        || fragmentCode.empty())
    {
        m_error =
            "Missing SDL GPU shader artifacts in "
            + shaderDirectory;

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

    SDL_GPUShader *vertexShader =
        SDL_CreateGPUShader(
            m_gpuDevice,
            &vertexInfo);

    SDL_GPUShader *fragmentShader =
        SDL_CreateGPUShader(
            m_gpuDevice,
            &fragmentInfo);

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

    /*
     * Base graphics pipeline.
     */

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

    /*
     * IMPORTANT:
     *
     * iOS Metal validation rejected the previous
     * D16 depth-enabled pipeline when it was bound
     * to the first real EngineSim render pass.
     *
     * EngineSim already explicitly sorts geometry
     * by render layer, so the iOS bring-up path can
     * safely run without a depth attachment.
     *
     * We can restore depth later with a Metal-native
     * attachment format once the entire application
     * is stable.
     */

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

    /*
     * Scene pipeline.
     */

    m_scenePipeline =
        SDL_CreateGPUGraphicsPipeline(
            m_gpuDevice,
            &pipelineInfo);

    /*
     * UI never needs depth.
     */

    pipelineInfo.target_info.depth_stencil_format =
        SDL_GPU_TEXTUREFORMAT_INVALID;

    pipelineInfo.target_info.has_depth_stencil_target =
        false;

    pipelineInfo.depth_stencil_state = {};

    m_uiPipeline =
        SDL_CreateGPUGraphicsPipeline(
            m_gpuDevice,
            &pipelineInfo);

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

        shutdown();

        return false;
    }

    SDL_Log(
        "EngineSim: SDL GPU pipelines created successfully.");

#if defined(ENGINE_SIM_IOS)

    SDL_Log(
        "EngineSim: iOS depth path disabled for Metal bring-up.");

#endif

    return true;
}

void SdlGpuRenderer::shutdown()
{
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

        m_vertexTransferBuffer = nullptr;
        m_indexTransferBuffer = nullptr;

        m_vertexBuffer = nullptr;
        m_indexBuffer = nullptr;

        m_scenePipeline = nullptr;
        m_uiPipeline = nullptr;

        m_sceneTexture = nullptr;
        m_depthTexture = nullptr;

        m_sceneTextureWidth = 0;
        m_sceneTextureHeight = 0;

        m_depthTextureWidth = 0;
        m_depthTextureHeight = 0;

        if (m_window != nullptr)
        {
            SDL_ReleaseWindowFromGPUDevice(
                m_gpuDevice,
                static_cast<SDL_Window *>(
                    m_window));
        }

        SDL_DestroyGPUDevice(
            m_gpuDevice);

        m_gpuDevice = nullptr;
    }

    m_window = nullptr;
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
    if (
        m_gpuDevice == nullptr
        || m_window == nullptr)
    {
        return;
    }

    SDL_GPUCommandBuffer *commands =
        SDL_AcquireGPUCommandBuffer(
            m_gpuDevice);

    if (commands == nullptr)
    {
        return;
    }

    /*
     * Upload generated EngineSim geometry.
     */

    if (
        m_vertexCount > 0
        && m_indexCount > 0)
    {
        void *vertexUpload =
            SDL_MapGPUTransferBuffer(
                m_gpuDevice,
                m_vertexTransferBuffer,
                true);

        void *indexUpload =
            SDL_MapGPUTransferBuffer(
                m_gpuDevice,
                m_indexTransferBuffer,
                true);

        if (
            vertexUpload != nullptr
            && indexUpload != nullptr)
        {
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
        }

        if (vertexUpload != nullptr)
        {
            SDL_UnmapGPUTransferBuffer(
                m_gpuDevice,
                m_vertexTransferBuffer);
        }

        if (indexUpload != nullptr)
        {
            SDL_UnmapGPUTransferBuffer(
                m_gpuDevice,
                m_indexTransferBuffer);
        }

        if (
            vertexUpload != nullptr
            && indexUpload != nullptr)
        {
            SDL_GPUCopyPass *copyPass =
                SDL_BeginGPUCopyPass(
                    commands);

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

                SDL_UploadToGPUBuffer(
                    copyPass,
                    &vertexSource,
                    &vertexDestination,
                    true);

                SDL_UploadToGPUBuffer(
                    copyPass,
                    &indexSource,
                    &indexDestination,
                    true);

                SDL_EndGPUCopyPass(
                    copyPass);
            }
        }
    }

    /*
     * Acquire the real iPhone Metal swapchain texture.
     */

    SDL_GPUTexture *swapchainTexture =
        nullptr;

    Uint32 swapchainWidth =
        0;

    Uint32 swapchainHeight =
        0;

    if (
        !SDL_WaitAndAcquireGPUSwapchainTexture(
            commands,
            static_cast<SDL_Window *>(
                m_window),
            &swapchainTexture,
            &swapchainWidth,
            &swapchainHeight))
    {
        SDL_CancelGPUCommandBuffer(
            commands);

        return;
    }

    if (swapchainTexture == nullptr)
    {
        SDL_SubmitGPUCommandBuffer(
            commands);

        return;
    }

    /*
     * Helper: issue EngineSim submissions for one stage.
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

        for (
            const Submission *submission
            : submissions)
        {
            const ysMatrix transforms[] =
            {
                submission->transform,
                submission->cameraView,
                submission->projection
            };

            SDL_PushGPUVertexUniformData(
                commands,
                0,
                transforms,
                sizeof(transforms));

            SDL_PushGPUFragmentUniformData(
                commands,
                0,
                &submission->color,
                sizeof(submission->color));

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
        }
    };

    const auto bindGeometry =
        [&](SDL_GPURenderPass *pass,
            SDL_GPUGraphicsPipeline *pipeline)
    {
        SDL_BindGPUGraphicsPipeline(
            pass,
            pipeline);

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

        SDL_BindGPUVertexBuffers(
            pass,
            0,
            &vertexBinding,
            1);

        SDL_BindGPUIndexBuffer(
            pass,
            &indexBinding,
            SDL_GPU_INDEXELEMENTSIZE_16BIT);
    };

#if defined(ENGINE_SIM_IOS)

    /*
     * ========================================================
     * iOS Metal path
     * ========================================================
     *
     * Render directly into the swapchain.
     *
     * This deliberately avoids the D16 offscreen/depth pass
     * that caused Metal validation to SIGABRT on the first
     * real EngineSim frame.
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

    SDL_GPURenderPass *pass =
        SDL_BeginGPURenderPass(
            commands,
            &colorTarget,
            1,
            nullptr);

    if (pass != nullptr)
    {
        /*
         * Real EngineSim mechanical scene.
         */

        if (
            m_scenePipeline != nullptr
            && m_vertexCount > 0
            && m_indexCount > 0)
        {
            bindGeometry(
                pass,
                m_scenePipeline);

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

                SDL_SetGPUViewport(
                    pass,
                    &viewport);
            }

            drawStage(
                pass,
                Shaders::SceneStage);
        }

        /*
         * Restore full-window viewport before UI.
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

        SDL_SetGPUViewport(
            pass,
            &fullViewport);

        /*
         * Real EngineSim gauges / oscilloscope / text / UI.
         */

        if (
            m_uiPipeline != nullptr
            && m_vertexCount > 0
            && m_indexCount > 0)
        {
            bindGeometry(
                pass,
                m_uiPipeline);

            drawStage(
                pass,
                Shaders::UiStage);
        }

        SDL_EndGPURenderPass(
            pass);
    }

#else

    /*
     * ========================================================
     * Existing desktop path
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

    SDL_SubmitGPUCommandBuffer(
        commands);
}

const char *SdlGpuRenderer::lastError() const
{
    return
        m_error.empty()
            ? SDL_GetError()
            : m_error.c_str();
}
