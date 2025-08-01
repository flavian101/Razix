// clang-format off
#include "rzxpch.h"
// clang-format on
#include "RZSkyboxPass.h"

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

        void RZSkyboxPass::addPass(RZFrameGraph& framegraph, Razix::RZScene* scene, RZRendererSettings* settings)
        {
            auto skyboxShader = RZShaderLibrary::Get().getBuiltInShader(ShaderBuiltin::Skybox);
            //            auto proceduralSkyboxShader = RZShaderLibrary::Get().getBuiltInShader(ShaderBuiltin::ProceduralSkybox);

            Gfx::RZPipelineDesc pipelineInfo{};
            pipelineInfo.name                   = "Skybox.Pipeline";
            pipelineInfo.cullMode               = Gfx::CullMode::Back;
            pipelineInfo.depthBiasEnabled       = false;
            pipelineInfo.drawType               = Gfx::DrawType::Triangle;
            pipelineInfo.shader                 = skyboxShader;
            pipelineInfo.transparencyEnabled    = true;
            pipelineInfo.colorAttachmentFormats = {Gfx::TextureFormat::RGBA16F};
            pipelineInfo.depthFormat            = Gfx::TextureFormat::DEPTH32F;
            pipelineInfo.depthTestEnabled       = true;
            pipelineInfo.depthWriteEnabled      = false;
            pipelineInfo.depthOp                = CompareOp::LessOrEqual;
            m_Pipeline                          = RZResourceManager::Get().createPipeline(pipelineInfo);

            // pipelineInfo.name    = "ProceduralSkybox.Pipeline";
            // pipelineInfo.shader  = proceduralSkyboxShader;
            // m_ProceduralPipeline = RZResourceManager::Get().createPipeline(pipelineInfo);
            // auto& volumetricData  = framegraph.getBlackboard().get<VolumetricCloudsData>();

            auto& frameDataBlock  = framegraph.getBlackboard().get<FrameData>();
            auto& lightProbesData = framegraph.getBlackboard().get<GlobalLightProbeData>();
            auto& sceneData       = framegraph.getBlackboard().get<SceneData>();
            auto& gBufferData     = framegraph.getBlackboard().get<GBufferData>();

            framegraph.getBlackboard().add<SkyboxPassData>() = framegraph.addCallbackPass<SkyboxPassData>(
                "Pass.Builtin.Code.Skybox",
                [&](SkyboxPassData& data, RZPassResourceBuilder& builder) {
                    builder
                        .setAsStandAlonePass()
                        .setDepartment(Department::Environment);

                    builder.read(frameDataBlock.frameData);
                    builder.read(lightProbesData.environmentMap);
                    builder.read(lightProbesData.diffuseIrradianceMap);
                    builder.read(lightProbesData.specularPreFilteredMap);
                    builder.read(gBufferData.GBufferDepth);

                    sceneData.SceneHDR = builder.write(sceneData.SceneHDR);
                    data.SceneHDR      = sceneData.SceneHDR;
                },
                [=](const SkyboxPassData& data, RZPassResourceDirectory& resources) {
                    RAZIX_PROFILE_FUNCTIONC(RZ_PROFILE_COLOR_GRAPHICS);

                    RETURN_IF_BIT_NOT_SET(settings->renderFeatures, RendererFeature_Skybox);

                    RAZIX_TIME_STAMP_BEGIN("Skybox Pass");
                    RAZIX_MARK_BEGIN("Skybox pass", float4(0.33f, 0.45f, 1.0f, 1.0f));

                    auto cmdBuffer = RHI::GetCurrentCommandBuffer();

                    RenderingInfo info    = {};
                    info.resolution       = Resolution::kWindow;
                    info.colorAttachments = {{resources.get<RZFrameGraphTexture>(data.SceneHDR).getHandle(), {false, ClearColorPresets::TransparentBlack}}};
                    info.depthAttachment  = {resources.get<RZFrameGraphTexture>(gBufferData.GBufferDepth).getHandle(), {false, ClearColorPresets::DepthOneToZero}};

                    RHI::BeginRendering(cmdBuffer, info);

                    // Set the Descriptor Set once rendering starts
                    if (RZFrameGraph::IsFirstFrame()) {
                        if (!settings->useProceduralSkybox) {
                            auto& shaderBindVars = Gfx::RZResourceManager::Get().getShaderResource(skyboxShader)->getBindVars();
                            auto  descriptor     = shaderBindVars["environmentMap"];
                            if (descriptor)
                                descriptor->texture = resources.get<RZFrameGraphTexture>(lightProbesData.environmentMap).getHandle();

                            Gfx::RZResourceManager::Get().getShaderResource(skyboxShader)->updateBindVarsHeaps();
                        } else {
                            //                            auto& shaderBindVars = Gfx::RZResourceManager::Get().getShaderResource(proceduralSkyboxShader)->getBindVars();
                            //                            auto  descriptor     = shaderBindVars["NoiseTexture"];
                            //                            if (descriptor)
                            //                                descriptor->texture = resources.get<RZFrameGraphTexture>(volumetricData.noiseTexture).getHandle();
                            //
                            //                            Gfx::RZResourceManager::Get().getShaderResource(proceduralSkyboxShader)->updateBindVarsHeaps();
                        }
                    }

                    if (!settings->useProceduralSkybox) {
                        Gfx::RHI::BindPipeline(m_Pipeline, cmdBuffer);
                        scene->drawScene(m_Pipeline, SceneDrawGeometryMode::Cubemap);
                    } else {
                        // Since no skybox, we update the directional light direction
                        //                        auto lights = scene->GetComponentsOfType<LightComponent>();
                        //                        // We use the first found directional light
                        //                        // TODO: Cache this
                        //                        RZLight dirLight;
                        //                        for (auto& lc: lights) {
                        //                            if (lc.light.getType() == LightType::DIRECTIONAL)
                        //                                dirLight = lc.light;
                        //                            break;
                        //                        }
                        //                        struct PCData
                        //                        {
                        //                            float3 worldSpaceLightPos;
                        //                            //u32       noiseTextureIdx;
                        //                        } data{};
                        //                        // FIXME: Use direction
                        //                        data.worldSpaceLightPos = dirLight.getPosition();
                        //                        //data.noiseTextureIdx    = resources.get<RZFrameGraphTexture>(volumetricData.noiseTexture).getHandle().getIndex();
                        //                        RZPushConstant pc;
                        //                        pc.data        = &data;
                        //                        pc.size        = sizeof(PCData);
                        //                        pc.shaderStage = ShaderStage::kPixel;
                        //
                        //                        Gfx::RHI::BindPipeline(m_ProceduralPipeline, cmdBuffer);
                        //                        RHI::BindPushConstant(m_ProceduralPipeline, cmdBuffer, pc);
                        //                        scene->drawScene(m_ProceduralPipeline, SceneDrawGeometryMode::Cubemap);
                    }

                    RHI::EndRendering(cmdBuffer);
                    RAZIX_MARK_END();
                    RAZIX_TIME_STAMP_END();
                });
        }

        void RZSkyboxPass::destroy()
        {
            RZResourceManager::Get().destroyPipeline(m_Pipeline);
            // RZResourceManager::Get().destroyPipeline(m_ProceduralPipeline);
        }
    }    // namespace Gfx
}    // namespace Razix
