// clang-format off
#include "rzxpch.h"
// clang-format on
#include "RZSSAOPass.h"

#include "Razix/Core/App/RZApplication.h"
#include "Razix/Core/Markers/RZMarkers.h"
#include "Razix/Core/RZEngine.h"

#include "Razix/Gfx/RHI/API/RZDrawCommandBuffer.h"
#include "Razix/Gfx/RHI/API/RZIndexBuffer.h"
#include "Razix/Gfx/RHI/API/RZPipeline.h"
#include "Razix/Gfx/RHI/API/RZShader.h"
#include "Razix/Gfx/RHI/API/RZTexture.h"
#include "Razix/Gfx/RHI/API/RZUniformBuffer.h"
#include "Razix/Gfx/RHI/API/RZVertexBuffer.h"

#include "Razix/Gfx/RHI/RHI.h"

#include "Razix/Gfx/Renderers/RZSystemBinding.h"

#include "Razix/Gfx/RZMesh.h"
#include "Razix/Gfx/RZMeshFactory.h"
#include "Razix/Gfx/RZShaderLibrary.h"

#include "Razix/Gfx/Passes/Data/GlobalData.h"

#include "Razix/Gfx/Resources/RZFrameGraphBuffer.h"
#include "Razix/Gfx/Resources/RZFrameGraphTexture.h"

#include "Razix/Scene/Components/RZComponents.h"

#include "Razix/Scene/RZScene.h"

namespace Razix {
    namespace Gfx {

        static float lerp(float a, float b, float f)
        {
            return a + f * (b - a);
        }

        void RZSSAOPass::addPass(RZFrameGraph& framegraph, Razix::RZScene* scene, RZRendererSettings* settings)
        {
            auto ssaoShader = RZShaderLibrary::Get().getBuiltInShader(ShaderBuiltin::SSAO);

            Gfx::RZPipelineDesc pipelineInfo{};
            pipelineInfo.name                   = "SSAO FX Pipeline";
            pipelineInfo.cullMode               = Gfx::CullMode::None;
            pipelineInfo.depthBiasEnabled       = false;
            pipelineInfo.drawType               = Gfx::DrawType::Triangle;
            pipelineInfo.shader                 = ssaoShader;
            pipelineInfo.transparencyEnabled    = false;
            pipelineInfo.colorAttachmentFormats = {Gfx::TextureFormat::R32F};
            pipelineInfo.depthTestEnabled       = false;
            pipelineInfo.depthWriteEnabled      = false;
            m_PreBlurPipeline                   = RZResourceManager::Get().createPipeline(pipelineInfo);

            auto& frameDataBlock = framegraph.getBlackboard().get<FrameData>();
            auto& gbufferData    = framegraph.getBlackboard().get<GBufferData>();

            auto& ssaoData = framegraph.getBlackboard().add<SSAOImportData>();

            // Generate some data to pass to SSAO shader
            std::default_random_engine            rndEngine((unsigned) time(nullptr));
            std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

            // Sample kernel
            std::vector<float4> ssaoKernel(SSAO_KERNEL_SIZE);
            for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i) {
                float3 sample(rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine));
                sample = normalize(sample);
                sample *= rndDist(rndEngine);
                float scale   = float(i) / float(SSAO_KERNEL_SIZE);
                scale         = lerp(0.1f, 1.0f, scale * scale);
                ssaoKernel[i] = float4(sample * scale, 0.0f);
            }

            // Random noise
            std::vector<float4> ssaoNoise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
            for (uint32_t i = 0; i < static_cast<uint32_t>(ssaoNoise.size()); i++) {
                ssaoNoise[i] = float4(rndDist(rndEngine) * 2.0f - 1.0f, rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
            }
            // SSAO kernel samples buffer
            RZBufferDesc samplesBufferDesc = {};
            samplesBufferDesc.name         = "Kernel";
            samplesBufferDesc.size         = static_cast<u32>(ssaoKernel.size()) * sizeof(float4);
            samplesBufferDesc.data         = ssaoKernel.data();
            samplesBufferDesc.usage        = BufferUsage::Static;
            auto ssaoKernelBuffer          = Gfx::RZResourceManager::Get().createUniformBuffer(samplesBufferDesc);
            ssaoData.SSAOKernelSamples     = framegraph.import <RZFrameGraphBuffer>(samplesBufferDesc.name, CAST_TO_FG_BUF_DESC samplesBufferDesc, {ssaoKernelBuffer});

            // SSAO Noise texture
            RZTextureDesc noiseTextureDesc{};
            noiseTextureDesc.name       = "SSAONoiseTex";
            noiseTextureDesc.width      = 4;
            noiseTextureDesc.height     = 4;
            noiseTextureDesc.data       = ssaoNoise.data();
            noiseTextureDesc.size       = static_cast<u32>(ssaoNoise.size()) * sizeof(float4);
            noiseTextureDesc.type       = TextureType::k2D;
            noiseTextureDesc.format     = TextureFormat::RGBA16F;
            noiseTextureDesc.enableMips = false;
            noiseTextureDesc.dataSize   = sizeof(float);
            auto ssaoNoiseTexture       = Gfx::RZResourceManager::Get().createTexture(noiseTextureDesc);
            ssaoData.SSAONoiseTexture   = framegraph.import <RZFrameGraphTexture>(noiseTextureDesc.name, CAST_TO_FG_TEX_DESC noiseTextureDesc, {ssaoNoiseTexture});

            framegraph.getBlackboard()
                .add<SSAOData>() = framegraph.addCallbackPass<SSAOData>(
                "Pass.Builtin.Code.FX.SSAO",
                [&](SSAOData& data, RZPassResourceBuilder& builder) {
                    builder.setAsStandAlonePass();

                    // We use a single channel f32 texture
                    RZTextureDesc textureDesc{};
                    textureDesc.name        = "SSAOPreBlurTexture";
                    textureDesc.width       = g_ResolutionToExtentsMap[Resolution::k1440p].x;
                    textureDesc.height      = g_ResolutionToExtentsMap[Resolution::k1440p].y;
                    textureDesc.type        = TextureType::k2D;
                    textureDesc.format      = TextureFormat::R32F;
                    data.SSAOPreBlurTexture = builder.create<RZFrameGraphTexture>(textureDesc.name, CAST_TO_FG_TEX_DESC textureDesc);

                    RZBufferDesc ssaoDataBufferDesc{};
                    ssaoDataBufferDesc.name  = "SSAOParams";
                    ssaoDataBufferDesc.size  = sizeof(SSAOParamsData);
                    ssaoDataBufferDesc.usage = BufferUsage::PersistentStream;
                    data.SSAOParams          = builder.create<RZFrameGraphBuffer>(ssaoDataBufferDesc.name, CAST_TO_FG_BUF_DESC ssaoDataBufferDesc);

                    data.SSAOPreBlurTexture = builder.write(data.SSAOPreBlurTexture);
                    data.SSAOParams         = builder.write(data.SSAOParams);

                    builder.read(ssaoData.SSAONoiseTexture);
                    builder.read(ssaoData.SSAOKernelSamples);
                    builder.read(frameDataBlock.frameData);
                    builder.read(gbufferData.GBuffer0);
                    builder.read(gbufferData.GBufferDepth);
                },
                [=](const SSAOData& data, RZPassResourceDirectory& resources) {
                    RAZIX_PROFILE_FUNCTIONC(RZ_PROFILE_COLOR_GRAPHICS);

                    RETURN_IF_BIT_NOT_SET(settings->renderFeatures, RendererFeature_SSAO);

                    RAZIX_TIME_STAMP_BEGIN("SSAO");
                    RAZIX_MARK_BEGIN("Pass.Builtin.Code.FX.SSAO", float4(178.0f, 190.0f, 181.0f, 255.0f) / 255.0f);

                    RenderingInfo info{};
                    info.resolution       = Resolution::kCustom;
                    info.colorAttachments = {{resources.get<RZFrameGraphTexture>(data.SSAOPreBlurTexture).getHandle(), {true, ClearColorPresets::OpaqueWhite}}};
                    info.extent           = {RZApplication::Get().getWindow()->getWidth(), RZApplication::Get().getWindow()->getHeight()};

                    RHI::BeginRendering(RHI::GetCurrentCommandBuffer(), info);

                    // Set the Descriptor Set once rendering starts
                    if (RZFrameGraph::IsFirstFrame()) {
                        auto& shaderBindVars = RZResourceManager::Get().getShaderResource(ssaoShader)->getBindVars();

                        RZDescriptor* descriptor = nullptr;

                        // Bind the GBuffer textures gBuffer0:Normal and SceneDepth
                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(gbufferData.GBuffer0)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(gbufferData.GBuffer0).getHandle();

                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(gbufferData.GBufferDepth)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(gbufferData.GBufferDepth).getHandle();

                        // SSAO Noise Texture
                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(ssaoData.SSAONoiseTexture)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(ssaoData.SSAONoiseTexture).getHandle();

                        // SSAO Kernel samples
                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphBuffer>(ssaoData.SSAOKernelSamples)];
                        if (descriptor)
                            descriptor->uniformBuffer = resources.get<RZFrameGraphBuffer>(ssaoData.SSAOKernelSamples).getHandle();

                        // SSAO Params
                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphBuffer>(data.SSAOParams)];
                        if (descriptor)
                            descriptor->uniformBuffer = resources.get<RZFrameGraphBuffer>(data.SSAOParams).getHandle();

                        RZResourceManager::Get().getShaderResource(ssaoShader)->updateBindVarsHeaps();
                    }

                    // Update the SSAO Data
                    SSAOParamsData ssaoData{};
                    ssaoData.radius     = 1.0f;
                    ssaoData.bias       = 0.025f;
                    ssaoData.resolution = float2(RZApplication::Get().getWindow()->getWidth(), RZApplication::Get().getWindow()->getHeight());
                    auto& cam           = scene->getSceneCamera();
                    // TODO: Bind FrameData @ slot 0
                    ssaoData.camViewPos       = cam.getPosition();
                    ssaoData.viewMatrix       = cam.getViewMatrix();
                    ssaoData.projectionMatrix = cam.getProjection();

                    auto ssaoDataHandle = resources.get<RZFrameGraphBuffer>(data.SSAOParams).getHandle();
                    RZResourceManager::Get().getUniformBufferResource(ssaoDataHandle)->SetData(sizeof(SSAOParamsData), &ssaoData);

                    RHI::BindPipeline(m_PreBlurPipeline, RHI::GetCurrentCommandBuffer());

                    scene->drawScene(m_PreBlurPipeline, SceneDrawGeometryMode::ScreenQuad);

                    RHI::EndRendering(RHI::GetCurrentCommandBuffer());
                    RAZIX_MARK_END();
                    RAZIX_TIME_STAMP_END();
                });
        }

        void RZSSAOPass::destroy()
        {
            RZResourceManager::Get().destroyPipeline(m_PreBlurPipeline);
        }
    }    // namespace Gfx
}    // namespace Razix