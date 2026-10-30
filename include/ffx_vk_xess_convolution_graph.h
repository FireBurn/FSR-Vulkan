/* SPDX-License-Identifier: MIT
 * Experimental command recorder for the recovered twelve-pass XeSS graph.
 * The caller owns all handles and synchronizes their lifetime. This does not
 * load models, create a device, submit work, or implement prepare/resolve.
 */
#ifndef FFX_VK_XESS_CONVOLUTION_GRAPH_H
#define FFX_VK_XESS_CONVOLUTION_GRAPH_H
#include <vulkan/vulkan.h>

typedef struct FfxVkXeSSConvolutionPass {
    VkPipeline pipeline;
    VkDescriptorSet descriptors;
    uint32_t constants[6];
    uint32_t groups[3];
} FfxVkXeSSConvolutionPass;

/* All images must already be GENERAL. Input/model writes must be visible to
 * compute shader reads before this call. Inter-pass dependencies remain on
 * the GPU. Output shader writes are made visible to subsequent compute reads;
 * consumers using other stages must add their own barrier. No host readback.
 * Pipelines/sets can be reused across frames after the caller synchronizes
 * reuse of the underlying tensors. Descriptor sets must match the recovered
 * 24-byte push-constant layout and six-element arrays at bindings 0..2.
 */
static inline VkResult ffxVkXeSSRecordConvolutions(VkCommandBuffer commands,
    VkPipelineLayout layout, const FfxVkXeSSConvolutionPass *passes,
    uint32_t count)
{
    if (!commands || !layout || !passes || count != 12)
        return VK_ERROR_INITIALIZATION_FAILED;
    /* Validate before recording any partial graph. Device-specific dispatch
     * limits and shader/descriptor compatibility remain caller requirements. */
    for (uint32_t i=0;i<count;++i) {
        if (!passes[i].pipeline || !passes[i].descriptors)
            return VK_ERROR_INITIALIZATION_FAILED;
        for (uint32_t j=0;j<3;++j)
            if (!passes[i].groups[j] || passes[i].groups[j] > 65535)
                return VK_ERROR_INITIALIZATION_FAILED;
    }
    for (uint32_t i=0;i<count;++i) {
        const FfxVkXeSSConvolutionPass *p=&passes[i];
        vkCmdBindPipeline(commands,VK_PIPELINE_BIND_POINT_COMPUTE,p->pipeline);
        vkCmdBindDescriptorSets(commands,VK_PIPELINE_BIND_POINT_COMPUTE,layout,
            0,1,&p->descriptors,0,0);
        vkCmdPushConstants(commands,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,24,p->constants);
        vkCmdDispatch(commands,p->groups[0],p->groups[1],p->groups[2]);
        VkMemoryBarrier barrier={VK_STRUCTURE_TYPE_MEMORY_BARRIER,0,
            VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
        vkCmdPipelineBarrier(commands,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,0,0,0);
    }
    return VK_SUCCESS;
}
#endif
