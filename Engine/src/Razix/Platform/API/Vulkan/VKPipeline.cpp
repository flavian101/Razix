// clang-format off
#include "rzxpch.h"
// clang-format on
#include "VKPipeline.h"

#include "Razix/Core/RZEngine.h"

#include "Razix/AssetSystem/RZAssetFileSpec.h"

#include "Razix/Platform/API/Vulkan/VKDevice.h"
#include "Razix/Platform/API/Vulkan/VKDrawCommandBuffer.h"
#include "Razix/Platform/API/Vulkan/VKShader.h"
#include "Razix/Platform/API/Vulkan/VKUtilities.h"

namespace Razix {
    namespace Gfx {

        VKPipeline::VKPipeline(const RZPipelineDesc& pipelineInfo RZ_DEBUG_NAME_TAG_E_ARG)
        {
            m_Desc           = pipelineInfo;
            auto shader      = RZResourceManager::Get().getPool<RZShader>().get(m_Desc.shader);
            m_PipelineLayout = static_cast<VKShader*>(shader)->getPipelineLayout();

            if (pipelineInfo.pipelineType == PipelineType::kGraphics)
                initGraphics(pipelineInfo RZ_DEBUG_E_ARG_NAME);
            else
                initCompute(pipelineInfo RZ_DEBUG_E_ARG_NAME);
        }

        RAZIX_CLEANUP_RESOURCE_IMPL(VKPipeline)
        {
            RAZIX_PROFILE_FUNCTIONC(RZ_PROFILE_COLOR_GRAPHICS);

            vkDestroyPipeline(VKDevice::Get().getDevice(), m_Pipeline, nullptr);
        }

        void VKPipeline::Bind(RZDrawCommandBufferHandle cmdBuffer)
        {
            RAZIX_PROFILE_FUNCTIONC(RZ_PROFILE_COLOR_GRAPHICS);

            auto                cmdBufferResource = RZResourceManager::Get().getDrawCommandBufferResource(cmdBuffer);
            VkPipelineBindPoint bindPoint         = m_Desc.pipelineType == PipelineType::kGraphics ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;
            vkCmdBindPipeline(static_cast<VKDrawCommandBuffer*>(cmdBufferResource)->getBuffer(), bindPoint, m_Pipeline);
        }

        void VKPipeline::initGraphics(const RZPipelineDesc& pipelineInfo RZ_DEBUG_NAME_TAG_E_ARG)
        {
            RAZIX_PROFILE_FUNCTIONC(RZ_PROFILE_COLOR_GRAPHICS);

            auto shader = RZResourceManager::Get().getPool<RZShader>().get(m_Desc.shader);

            //----------------------------
            // Vertex Input Layout Stage
            //----------------------------
#if RAZIX_ASSET_VERSION == RAZIX_ASSET_VERSION_V1
            VkVertexInputBindingDescription vertexBindingDescription{};
            vertexBindingDescription.binding   = 0;
            vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            vertexBindingDescription.stride    = shader->getInputStride();
#endif

            // Get the input description information from the shader reflection
            auto& vertexInputBindingDescription   = static_cast<VKShader*>(shader)->getVertexBindingDescriptions();
            auto& vertexInputAttributeDescription = static_cast<VKShader*>(shader)->getVertexAttribDescriptions();

            VkPipelineVertexInputStateCreateInfo vertexInputSCI{};
            vertexInputSCI.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputSCI.pNext                           = nullptr;
            vertexInputSCI.vertexBindingDescriptionCount   = u32(vertexInputBindingDescription.size());
            vertexInputSCI.pVertexBindingDescriptions      = vertexInputBindingDescription.data();
            vertexInputSCI.vertexAttributeDescriptionCount = u32(vertexInputAttributeDescription.size());
            vertexInputSCI.pVertexAttributeDescriptions    = vertexInputAttributeDescription.data();

            //----------------------------
            // Input Assembly Stage
            //----------------------------
            VkPipelineInputAssemblyStateCreateInfo inputAssemblySCI{};
            inputAssemblySCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssemblySCI.pNext                  = nullptr;
            inputAssemblySCI.primitiveRestartEnable = VK_FALSE;
            inputAssemblySCI.topology               = VKUtilities::DrawTypeToVK(pipelineInfo.drawType);

            //----------------------------
            // Viewport and Dynamic states
            //----------------------------

            std::vector<VkDynamicState> dynamicStateDescriptors;

            VkPipelineViewportStateCreateInfo viewportSCI{};
            viewportSCI.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportSCI.pNext         = nullptr;
            viewportSCI.viewportCount = 1;
            viewportSCI.scissorCount  = 1;
            viewportSCI.pScissors     = nullptr;
            viewportSCI.pViewports    = nullptr;
            dynamicStateDescriptors.push_back(VK_DYNAMIC_STATE_VIEWPORT);
            dynamicStateDescriptors.push_back(VK_DYNAMIC_STATE_SCISSOR);

            if (pipelineInfo.depthBiasEnabled)
                dynamicStateDescriptors.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);

            VkPipelineDynamicStateCreateInfo dynamicStateCI{};
            dynamicStateCI.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateCI.pNext             = NULL;
            dynamicStateCI.dynamicStateCount = u32(dynamicStateDescriptors.size());
            dynamicStateCI.pDynamicStates    = dynamicStateDescriptors.data();

            //----------------------------
            // Rasterizer Stage
            //----------------------------
            VkPipelineRasterizationStateCreateInfo rasterizationSCI{};
            rasterizationSCI.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizationSCI.pNext                   = nullptr;
            rasterizationSCI.cullMode                = VKUtilities::CullModeToVK(pipelineInfo.cullMode);
            rasterizationSCI.depthBiasClamp          = 0;
            rasterizationSCI.depthBiasConstantFactor = 0;
            rasterizationSCI.depthBiasEnable         = (pipelineInfo.depthBiasEnabled ? VK_TRUE : VK_FALSE);
            rasterizationSCI.depthBiasSlopeFactor    = 0;
            rasterizationSCI.depthClampEnable        = VK_FALSE;
            rasterizationSCI.frontFace               = VK_FRONT_FACE_CLOCKWISE;
            rasterizationSCI.lineWidth               = 1.0f;
            rasterizationSCI.polygonMode             = VKUtilities::PolygoneModeToVK(pipelineInfo.polygonMode);
            rasterizationSCI.rasterizerDiscardEnable = VK_FALSE;

            //----------------------------
            // Blend State Stage
            //----------------------------
            VkPipelineColorBlendStateCreateInfo colorBlendSCI{};
            colorBlendSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendSCI.pNext = NULL;
            colorBlendSCI.flags = 0;

            std::vector<VkPipelineColorBlendAttachmentState> blendAttachState;

            blendAttachState.resize(pipelineInfo.colorAttachmentFormats.size());

            for (unsigned int i = 0; i < blendAttachState.size(); i++) {
                blendAttachState[i]                = VkPipelineColorBlendAttachmentState();
                blendAttachState[i].colorWriteMask = 0x0f;
                blendAttachState[i].colorBlendOp   = VKUtilities::BlendOpToVK(pipelineInfo.colorOp);
                blendAttachState[i].alphaBlendOp   = VKUtilities::BlendOpToVK(pipelineInfo.alphaOp);

                if (pipelineInfo.transparencyEnabled) {
                    blendAttachState[i].blendEnable         = VK_TRUE;
                    blendAttachState[i].srcColorBlendFactor = VKUtilities::BlendFactorToVK(pipelineInfo.colorSrc);
                    blendAttachState[i].dstColorBlendFactor = VKUtilities::BlendFactorToVK(pipelineInfo.colorDst);
                    blendAttachState[i].srcAlphaBlendFactor = VKUtilities::BlendFactorToVK(pipelineInfo.alphaSrc);
                    blendAttachState[i].dstAlphaBlendFactor = VKUtilities::BlendFactorToVK(pipelineInfo.alphaDst);
                } else {
                    blendAttachState[i].blendEnable         = VK_FALSE;
                    blendAttachState[i].srcColorBlendFactor = VKUtilities::BlendFactorToVK(pipelineInfo.colorSrc);
                    blendAttachState[i].dstColorBlendFactor = VKUtilities::BlendFactorToVK(pipelineInfo.colorDst);
                    blendAttachState[i].srcAlphaBlendFactor = VKUtilities::BlendFactorToVK(pipelineInfo.alphaSrc);
                    blendAttachState[i].dstAlphaBlendFactor = VKUtilities::BlendFactorToVK(pipelineInfo.alphaDst);
                }
            }

            colorBlendSCI.attachmentCount   = static_cast<u32>(blendAttachState.size());
            colorBlendSCI.pAttachments      = blendAttachState.data();
            colorBlendSCI.logicOpEnable     = VK_FALSE;
            colorBlendSCI.logicOp           = VK_LOGIC_OP_NO_OP;
            colorBlendSCI.blendConstants[0] = 1.0f;
            colorBlendSCI.blendConstants[1] = 1.0f;
            colorBlendSCI.blendConstants[2] = 1.0f;
            colorBlendSCI.blendConstants[3] = 1.0f;

            //----------------------------
            // Depth Stencil Stage
            //----------------------------
            VkPipelineDepthStencilStateCreateInfo depthStencilSCI{};
            depthStencilSCI.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencilSCI.pNext                 = nullptr;
            depthStencilSCI.depthTestEnable       = pipelineInfo.depthTestEnabled;
            depthStencilSCI.depthWriteEnable      = pipelineInfo.depthWriteEnabled;
            depthStencilSCI.depthCompareOp        = VKUtilities::CompareOpToVK(pipelineInfo.depthOp);
            depthStencilSCI.depthBoundsTestEnable = VK_FALSE;

            // Stencil Testing is always disabled so no need to care about it's operations
            depthStencilSCI.stencilTestEnable = VK_FALSE;

            depthStencilSCI.back.failOp      = VK_STENCIL_OP_KEEP;
            depthStencilSCI.back.passOp      = VK_STENCIL_OP_KEEP;
            depthStencilSCI.back.compareOp   = VK_COMPARE_OP_ALWAYS;
            depthStencilSCI.back.compareMask = 0;
            depthStencilSCI.back.reference   = 0;
            depthStencilSCI.back.depthFailOp = VK_STENCIL_OP_KEEP;
            depthStencilSCI.back.writeMask   = 0;

            depthStencilSCI.minDepthBounds = 0;
            depthStencilSCI.maxDepthBounds = 0;
            depthStencilSCI.front          = depthStencilSCI.back;

            //----------------------------
            // Multi sample State (MSAA)
            //----------------------------
            VkPipelineMultisampleStateCreateInfo multiSampleSCI{};
            multiSampleSCI.sType       = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multiSampleSCI.pNext       = nullptr;
            multiSampleSCI.pSampleMask = nullptr;
            // Razix::RZEngine::Get().getGlobalEngineSettings().EnableMSAA use to apply more samples
            multiSampleSCI.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
            multiSampleSCI.sampleShadingEnable   = VK_FALSE;
            multiSampleSCI.alphaToCoverageEnable = VK_FALSE;
            multiSampleSCI.alphaToOneEnable      = VK_FALSE;
            multiSampleSCI.minSampleShading      = 0.5;

            //----------------------------
            // Dynamic Rendering KHR
            //----------------------------

            VkPipelineRenderingCreateInfoKHR renderingCI{};
            renderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
            std::vector<VkFormat> formats;
            for (auto& attachment: pipelineInfo.colorAttachmentFormats)
                formats.push_back(VKUtilities::TextureFormatToVK(attachment));
            renderingCI.colorAttachmentCount    = static_cast<u32>(pipelineInfo.colorAttachmentFormats.size());
            renderingCI.pColorAttachmentFormats = formats.data();
            renderingCI.depthAttachmentFormat   = VKUtilities::TextureFormatToVK(pipelineInfo.depthFormat);    // defaults to VK_FORMAT_UNDEFINED

            //----------------------------
            // Graphics Pipeline
            //----------------------------
            VkGraphicsPipelineCreateInfo graphicsPipelineCI{};
            graphicsPipelineCI.sType                                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            graphicsPipelineCI.pNext                                  = &renderingCI;
            graphicsPipelineCI.layout                                 = m_PipelineLayout;
            graphicsPipelineCI.basePipelineHandle                     = VK_NULL_HANDLE;
            graphicsPipelineCI.basePipelineIndex                      = -1;
            graphicsPipelineCI.pVertexInputState                      = &vertexInputSCI;
            graphicsPipelineCI.pInputAssemblyState                    = &inputAssemblySCI;
            graphicsPipelineCI.pRasterizationState                    = &rasterizationSCI;
            graphicsPipelineCI.pColorBlendState                       = &colorBlendSCI;
            graphicsPipelineCI.pTessellationState                     = nullptr;
            graphicsPipelineCI.pMultisampleState                      = &multiSampleSCI;
            graphicsPipelineCI.pDynamicState                          = &dynamicStateCI;
            graphicsPipelineCI.pViewportState                         = &viewportSCI;
            graphicsPipelineCI.pDepthStencilState                     = &depthStencilSCI;
            std::vector<VkPipelineShaderStageCreateInfo> shaderStages = static_cast<VKShader*>(shader)->getShaderStages();
            graphicsPipelineCI.pStages                                = shaderStages.data();
            graphicsPipelineCI.stageCount                             = static_cast<u32>(shaderStages.size());
            graphicsPipelineCI.renderPass                             = VK_NULL_HANDLE;    //static_cast<VKRenderPass*>(pipelineInfo.renderpass)->getVKRenderPass();

            // TODO: use pipeline cache
            if (VK_CHECK_RESULT(vkCreateGraphicsPipelines(VKDevice::Get().getDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineCI, nullptr, &m_Pipeline)))
                RAZIX_CORE_ERROR("[Vulkan] Cannot create graphics pipeline!");
            else
                RAZIX_CORE_TRACE("[Vulkan] Successfully created graphics pipeline!");

            VK_TAG_OBJECT(bufferName, VK_OBJECT_TYPE_PIPELINE, (uint64_t) m_Pipeline);
        }

        void VKPipeline::initCompute(const RZPipelineDesc& pipelineInfo RZ_DEBUG_NAME_TAG_E_ARG)
        {
            auto shader = RZResourceManager::Get().getPool<RZShader>().get(m_Desc.shader);

            VkComputePipelineCreateInfo computePipelineCI = {};
            computePipelineCI.sType                       = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            computePipelineCI.layout                      = m_PipelineLayout;
            computePipelineCI.flags                       = 0;
            computePipelineCI.stage                       = static_cast<VKShader*>(shader)->getShaderStages()[0];    // CS is the only stage for this shader

            if (VK_CHECK_RESULT(vkCreateComputePipelines(VKDevice::Get().getDevice(), VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &m_Pipeline)))
                RAZIX_CORE_ERROR("[Vulkan] Cannot create compute pipeline!");
            else
                RAZIX_CORE_TRACE("[Vulkan] Successfully created compute pipeline!");

            VK_TAG_OBJECT(bufferName, VK_OBJECT_TYPE_PIPELINE, (uint64_t) m_Pipeline);
        }
    }    // namespace Gfx
}    // namespace Razix
