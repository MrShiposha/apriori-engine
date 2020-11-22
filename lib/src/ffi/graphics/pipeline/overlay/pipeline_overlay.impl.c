#include <stddef.h>

#include "mod.h"
#include "pipeline_overlay.impl.h"
#include "vertex_overlay.h"
#include "gpu_info.h"

#include "ffi/core/log.h"
#include "ffi/util/mod.h"
#include "ffi/graphics/renderer/mod.h"

#include "ffi/generated/gpu/overlay/vertex_overlay.h"
#include "ffi/generated/gpu/overlay/fragment_overlay.h"

#define LOG_TARGET LOG_STRUCT_TARGET(PipelineOVL)

Result new_pipeline_ovl(
    VkDevice device,
    VkRenderPass render_pass,
    const VkExtent2D *render_target_extent,
    uint32_t render_target_count,
    float *anisotropy
) {
    ASSERT_NOT_NULL(device);
    ASSERT_NOT_NULL(render_pass);
    ASSERT_NOT_NULL(render_target_extent);

    Result result = { 0 };
    struct PipelineOVL *pipeline = NULL;

    VkShaderModuleCreateInfo vertex_shader_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    };
    vertex_shader_ci.codeSize = vertex_overlay_code_size();
    vertex_shader_ci.pCode = vertex_overlay();

    VkShaderModuleCreateInfo fragment_shader_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    };
    fragment_shader_ci.codeSize = fragment_overlay_code_size();
    fragment_shader_ci.pCode = fragment_overlay();

    VkVertexInputBindingDescription vertex_binding_descr = {
        .binding = OVL_VERTEX_INPUT_BINDING,
        .stride = sizeof(struct VertexOVL),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription vertex_attr_descrs[] = {
        {
            .binding = OVL_VERTEX_INPUT_BINDING,
            .location = OVL_VERTEX_INPUT_LOCATION_POS,
            .offset = offsetof(struct VertexOVL, pos),
            .format = VK_FORMAT_R32G32_SFLOAT
        },
        {
            .binding = OVL_VERTEX_INPUT_BINDING,
            .location = OVL_VERTEX_INPUT_LOCATION_COLOR,
            .offset = offsetof(struct VertexOVL, color),
            .format = VK_FORMAT_R32G32B32A32_SFLOAT
        },
        {
            .binding = OVL_VERTEX_INPUT_BINDING,
            .location = OVL_VERTEX_INPUT_LOCATION_TEXTURE,
            .offset = offsetof(struct VertexOVL, tex),
            .format = VK_FORMAT_R32G32_SFLOAT
        },
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .vertexAttributeDescriptionCount = STATIC_ARRAY_SIZE(vertex_attr_descrs),
    };
    vertex_input_state_ci.pVertexBindingDescriptions = &vertex_binding_descr;
    vertex_input_state_ci.pVertexAttributeDescriptions = vertex_attr_descrs;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkRect2D scissor = {
        .offset = {0, 0}
    };
    scissor.extent = *render_target_extent;

    VkPipelineViewportStateCreateInfo viewport_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .scissorCount = 1
    };
    viewport_ci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisample_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState *color_blend_attachments = ALLOC_ARRAY(
        result,
        VkPipelineColorBlendAttachmentState,
        render_target_count
    );
    color_blend_attachments->colorWriteMask = VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachments->blendEnable = VK_TRUE;

    // <out-rgb> = <new-alpha> * <new-color> + (1 - <new-alpha>) * <old-color>
    color_blend_attachments->colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachments->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachments->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    color_blend_attachments->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachments->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    VkPipelineColorBlendStateCreateInfo color_blend_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    };
    color_blend_ci.attachmentCount = render_target_count;
    color_blend_ci.pAttachments = color_blend_attachments;

    VkDynamicState dyn_viewport = VK_DYNAMIC_STATE_VIEWPORT;

    VkPipelineDynamicStateCreateInfo dyn_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 1,
    };
    dyn_state_ci.pDynamicStates = &dyn_viewport;

    VkSamplerCreateInfo sampler_ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .anisotropyEnable = IS_NOT_NULL(anisotropy),
        .maxAnisotropy = UNWRAP_NULL_OR(anisotropy, 0.0f),
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_TRUE
    };

    VkPushConstantRange ui_plane_position_pcr = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(float2)
    };

    VkPushConstantRange ui_plane_extent_pcr = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = sizeof(float2),
        .size = sizeof(VkExtent2D)
    };

    VkPushConstantRange push_constant_ranges[] = {
        ui_plane_position_pcr,
        ui_plane_extent_pcr
    };

    VkDescriptorSetLayoutCreateInfo descr_set_layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    };

    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    };

    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .subpass = RENDER_SUBPASS_OVERLAY_IDX
    };

    info(LOG_TARGET, "creating new pipeline overlay...");

    pipeline = ALLOC(result, struct PipelineOVL);
    pipeline->device = device;

    result.error = vkCreateShaderModule(
        device,
        &vertex_shader_ci,
        NULL,
        &pipeline->vertex_shader
    );
    EXPECT_SUCCESS(result);

    result.error = vkCreateShaderModule(
        device,
        &fragment_shader_ci,
        NULL,
        &pipeline->fragment_shader
    );
    EXPECT_SUCCESS(result);

    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .pName = SHADER_DEFAULT_ENTRY_POINT
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pName = SHADER_DEFAULT_ENTRY_POINT
        }
    };
    stages[0].module = pipeline->vertex_shader;
    stages[1].module = pipeline->fragment_shader;

    result.error = vkCreateSampler(
        device,
        &sampler_ci,
        NULL,
        &pipeline->sampler
    );
    EXPECT_SUCCESS(result);

    VkDescriptorSetLayoutBinding set_layout_bindings[] = {
        {
            .binding = OVL_COMBINED_IMAGE_SAMPLER_DESCR_BINDING,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        }
    };
    set_layout_bindings[0].pImmutableSamplers = &pipeline->sampler;

    descr_set_layout_ci.bindingCount = STATIC_ARRAY_SIZE(set_layout_bindings);
    descr_set_layout_ci.pBindings = set_layout_bindings;

    result.error = vkCreateDescriptorSetLayout(
        device,
        &descr_set_layout_ci,
        NULL,
        &pipeline->descr_set_layout
    );
    EXPECT_SUCCESS(result);

    layout_ci.setLayoutCount = 1;
    layout_ci.pSetLayouts = &pipeline->descr_set_layout;
    layout_ci.pushConstantRangeCount = STATIC_ARRAY_SIZE(push_constant_ranges);
    layout_ci.pPushConstantRanges = push_constant_ranges;

    result.error = vkCreatePipelineLayout(
        device,
        &layout_ci,
        NULL,
        &pipeline->layout
    );
    EXPECT_SUCCESS(result);

    pipeline_ci.stageCount = STATIC_ARRAY_SIZE(stages);
    pipeline_ci.pStages = stages;
    pipeline_ci.pVertexInputState = &vertex_input_state_ci;
    pipeline_ci.pInputAssemblyState = &input_assembly_ci;
    pipeline_ci.pViewportState = &viewport_ci;
    pipeline_ci.pRasterizationState = &raster_state_ci;
    pipeline_ci.pMultisampleState = &multisample_ci;
    pipeline_ci.pColorBlendState = &color_blend_ci;
    pipeline_ci.pDynamicState = &dyn_state_ci;
    pipeline_ci.layout = pipeline->layout;
    pipeline_ci.renderPass = render_pass;

    info(LOG_TARGET, "new pipeline overlay created successfully");
    result.object = pipeline;

    FN_EXIT(result, {
        free(color_blend_attachments);
    });

    FN_FAILURE(result, {
        drop_pipeline_ovl(pipeline);
    });
}

void drop_pipeline_ovl(struct PipelineOVL *pipeline) {
    if (pipeline == NULL)
        goto exit;

    vkDestroyPipeline(
        pipeline->device,
        pipeline->vk_handle,
        NULL
    );

    vkDestroyPipelineLayout(
        pipeline->device,
        pipeline->layout,
        NULL
    );

    vkDestroyDescriptorSetLayout(
        pipeline->device,
        pipeline->descr_set_layout,
        NULL
    );

    vkDestroySampler(
        pipeline->device,
        pipeline->sampler,
        NULL
    );

    vkDestroyShaderModule(
        pipeline->device,
        pipeline->fragment_shader,
        NULL
    );

    vkDestroyShaderModule(
        pipeline->device,
        pipeline->vertex_shader,
        NULL
    );

exit:
    debug(LOG_TARGET, "drop pipeline OVL");
}

Result get_ovl_descriptor_pool_sizes(uint32_t render_target_count) {
    Result result = { 0 };
    const size_t needed_descriptor_count = render_target_count;

    VkDescriptorPoolSize *pool_sizes = ALLOC_ARRAY_UNINIT(
        result,
        VkDescriptorPoolSize,
        needed_descriptor_count
    );

    pool_sizes[OVL_COMBINED_IMAGE_SAMPLER_DESCR_IDX].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[OVL_COMBINED_IMAGE_SAMPLER_DESCR_IDX].descriptorCount = render_target_count;

    FN_FORCE_EXIT(result);
}
