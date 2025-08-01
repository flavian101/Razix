// clang-format off
#include "rzxpch.h"
// clang-format on
#include "RZPBRDeferredShadingPass.h"

#include "Razix/Core/App/RZApplication.h"
#include "Razix/Core/Markers/RZMarkers.h"
#include "Razix/Core/RZEngine.h"

#include "Razix/Gfx/RHI/API/RZDrawCommandBuffer.h"
#include "Razix/Gfx/RHI/API/RZGraphicsContext.h"
#include "Razix/Gfx/RHI/API/RZIndexBuffer.h"
#include "Razix/Gfx/RHI/API/RZPipeline.h"
#include "Razix/Gfx/RHI/API/RZShader.h"
#include "Razix/Gfx/RHI/API/RZTexture.h"
#include "Razix/Gfx/RHI/API/RZUniformBuffer.h"
#include "Razix/Gfx/RHI/API/RZVertexBuffer.h"

#include "Razix/Gfx/RHI/RHI.h"

#include "Razix/Gfx/RZMesh.h"
#include "Razix/Gfx/RZMeshFactory.h"

#include "Razix/Gfx/RZShaderLibrary.h"

#include "Razix/Gfx/Materials/RZMaterial.h"

#include "Razix/Gfx/Passes/Data/GlobalData.h"

#include "Razix/Gfx/Resources/RZFrameGraphBuffer.h"

#include "Razix/Gfx/Resources/RZFrameGraphTexture.h"

#include "Razix/Scene/Components/RZComponents.h"

#include "Razix/Scene/RZScene.h"

namespace Razix {
    namespace Gfx {

        void RZPBRDeferredShadingPass::addPass(RZFrameGraph& framegraph, Razix::RZScene* scene, RZRendererSettings* settings)
        {
            auto pbrShader = RZShaderLibrary::Get().getBuiltInShader(ShaderBuiltin::PBRDeferredLighting);

            Gfx::RZPipelineDesc pipelineInfo    = {};
            pipelineInfo.name                   = "Pipeline.PBRDeferredLighting";
            pipelineInfo.cullMode               = Gfx::CullMode::None;
            pipelineInfo.depthBiasEnabled       = false;
            pipelineInfo.drawType               = Gfx::DrawType::Triangle;
            pipelineInfo.shader                 = pbrShader;
            pipelineInfo.transparencyEnabled    = false;
            pipelineInfo.colorAttachmentFormats = {Gfx::TextureFormat::RGBA16F};
            pipelineInfo.depthTestEnabled       = false;
            pipelineInfo.depthWriteEnabled      = false;
            m_Pipeline                          = RZResourceManager::Get().createPipeline(pipelineInfo);

            auto& frameDataBlock       = framegraph.getBlackboard().get<FrameData>();
            auto& sceneLightsDataBlock = framegraph.getBlackboard().get<SceneLightsData>();
            auto& shadowData           = framegraph.getBlackboard().get<SimpleShadowPassData>();
            //auto& csmData              = framegraph.getBlackboard().get<CSMData>();
            auto& globalLightProbes = framegraph.getBlackboard().get<GlobalLightProbeData>();
            auto& brdfData          = framegraph.getBlackboard().get<BRDFData>();
            auto& gBufferData       = framegraph.getBlackboard().get<GBufferData>();

            framegraph.getBlackboard().add<SceneData>() = framegraph.addCallbackPass<SceneData>(
                "Pass.Builtin.Code.PBRDeferredLighting",
                [&](SceneData& data, RZPassResourceBuilder& builder) {
                    builder
                        .setAsStandAlonePass()
                        .setDepartment(Department::Lighting);

                    RZTextureDesc textureDesc         = {};
                    textureDesc.name                  = "SceneHDR";
                    textureDesc.width                 = RZApplication::Get().getWindow()->getWidth();
                    textureDesc.height                = RZApplication::Get().getWindow()->getHeight();
                    textureDesc.type                  = TextureType::k2D;
                    textureDesc.initResourceViewHints = kSRV | kRTV;
                    textureDesc.format                = TextureFormat::RGBA16F;
                    textureDesc.allowResize           = true;

                    data.SceneHDR = builder.create<RZFrameGraphTexture>(textureDesc.name, CAST_TO_FG_TEX_DESC textureDesc);
                    data.SceneHDR = builder.write(data.SceneHDR);

                    builder.read(frameDataBlock.frameData);
                    builder.read(sceneLightsDataBlock.lightsDataBuffer);
                    builder.read(shadowData.shadowMap);
                    builder.read(shadowData.lightVP);
                    builder.read(globalLightProbes.diffuseIrradianceMap);
                    builder.read(globalLightProbes.specularPreFilteredMap);
                    builder.read(gBufferData.GBuffer0);
                    builder.read(gBufferData.GBuffer1);
                    builder.read(gBufferData.GBuffer2);
                    builder.read(gBufferData.GBufferDepth);
                    builder.read(brdfData.lut);
                },
                [=](const SceneData& data, RZPassResourceDirectory& resources) {
                    RAZIX_PROFILE_FUNCTIONC(RZ_PROFILE_COLOR_GRAPHICS);

                    RAZIX_TIME_STAMP_BEGIN("Deferred Shading (PBR)");
                    RAZIX_MARK_BEGIN("Pass.Builtin.DeferredShading", float4(1.0f, 0.5f, 0.0f, 1.0f));

                    RenderingInfo info{};
                    info.resolution       = Resolution::kCustom;
                    info.colorAttachments = {{resources.get<RZFrameGraphTexture>(data.SceneHDR).getHandle(), {true, ClearColorPresets::TransparentBlack}}};
                    info.depthAttachment  = {resources.get<RZFrameGraphTexture>(gBufferData.GBufferDepth).getHandle(), {false, ClearColorPresets::DepthOneToZero}};
                    info.extent           = {RZApplication::Get().getWindow()->getWidth(), RZApplication::Get().getWindow()->getHeight()};

                    RHI::BeginRendering(RHI::GetCurrentCommandBuffer(), info);

                    // Set the Descriptor Set once rendering starts
                    if (RZFrameGraph::IsFirstFrame()) {
                        auto& shaderBindVars = RZResourceManager::Get().getShaderResource(pbrShader)->getBindVars();

                        RZDescriptor* descriptor = nullptr;

                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(shadowData.shadowMap)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(shadowData.shadowMap).getHandle();

                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphBuffer>(shadowData.lightVP)];
                        if (descriptor)
                            descriptor->uniformBuffer = resources.get<RZFrameGraphBuffer>(shadowData.lightVP).getHandle();

                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(globalLightProbes.diffuseIrradianceMap)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(globalLightProbes.diffuseIrradianceMap).getHandle();

                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(globalLightProbes.specularPreFilteredMap)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(globalLightProbes.specularPreFilteredMap).getHandle();

                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(brdfData.lut)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(brdfData.lut).getHandle();

                        // Bind the GBuffer textures
                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(gBufferData.GBuffer0)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(gBufferData.GBuffer0).getHandle();

                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(gBufferData.GBuffer1)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(gBufferData.GBuffer1).getHandle();

                        descriptor = shaderBindVars[resources.getResourceName<RZFrameGraphTexture>(gBufferData.GBuffer2)];
                        if (descriptor)
                            descriptor->texture = resources.get<RZFrameGraphTexture>(gBufferData.GBuffer2).getHandle();

                        // CSM Array Texture
                        //descriptor = shaderBindVars["CSMArray"];
                        //if (descriptor)
                        //    descriptor->texture = resources.get<RZFrameGraphTexture>(csmData.cascadedShadowMaps).getHandle();

                        // CSM Matrices
                        //descriptor = shaderBindVars["CSMMatrices"];
                        //if (descriptor)
                        //    descriptor->uniformBuffer = resources.get<RZFrameGraphBuffer>(csmData.viewProjMatrices).getHandle();

                        RZResourceManager::Get().getShaderResource(pbrShader)->updateBindVarsHeaps();
                    }

                    RHI::BindPipeline(m_Pipeline, RHI::GetCurrentCommandBuffer());

                    struct PCData
                    {
                        float3   CameraViewPos;
                        f32      dt;
                        float4x4 camView;
                    } pcData             = {};
                    pcData.CameraViewPos = scene->getSceneCamera().getPosition();
                    pcData.camView       = scene->getSceneCamera().getViewMatrix();
                    pcData.dt            = RZEngine::Get().GetStatistics().DeltaTime;
                    RZPushConstant pc;
                    pc.size        = sizeof(PCData);
                    pc.data        = &pcData;
                    pc.shaderStage = ShaderStage::kPixel;
                    RHI::BindPushConstant(m_Pipeline, RHI::GetCurrentCommandBuffer(), pc);

                    scene->drawScene(m_Pipeline, SceneDrawGeometryMode::ScreenQuad);

                    RHI::EndRendering(RHI::GetCurrentCommandBuffer());
                    RAZIX_MARK_END();
                    RAZIX_TIME_STAMP_END();
                });
        }

        void RZPBRDeferredShadingPass::destroy()
        {
            RZResourceManager::Get().destroyPipeline(m_Pipeline);
        }
    }    // namespace Gfx
}    // namespace Razix
