// SPDX-License-Identifier: MIT
#pragma once
#include "ffx_vk_xess_fixed_context.hpp"
#include "ffx_vk_xess_recovered.h"
#include "xess_input_conversion_spv.h"
#include <cmath>
#include <iterator>

namespace ffxvk {
class XeSSInputConversion final {
    VkDevice device_;
    FfxVkXeSSReplayExtent extent_;
    XeSSResources resources_;
    struct Slot {
        XeSSImage color,motion,depth,output;
        XeSSStage before,after;
        FfxVkXeSSRecoveredConversion conversion{};
        bool initialized=false;
    };
    std::vector<Slot> slots_;
    struct Push {uint32_t width,height;float motionX,motionY,colorScale;};
    static_assert(sizeof(Push)==20,"Conversion push constant ABI");
    static void require(bool value,const char* reason) {if(!value) throw std::invalid_argument(reason);}
    void bindImage(const XeSSStage& stage,uint32_t binding,VkDescriptorType type,VkImageView view) {
        VkDescriptorImageInfo info{VK_NULL_HANDLE,view,VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet write{};write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet=stage.set;write.dstBinding=binding;write.descriptorCount=1;
        write.descriptorType=type;write.pImageInfo=&info;vkUpdateDescriptorSets(device_,1,&write,0,nullptr);
    }
    static void barrier(VkCommandBuffer cmd,VkPipelineStageFlags source,VkPipelineStageFlags target,
        VkAccessFlags from,VkAccessFlags to) {
        VkMemoryBarrier b{};b.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;b.srcAccessMask=from;b.dstAccessMask=to;
        vkCmdPipelineBarrier(cmd,source,target,0,1,&b,0,nullptr,0,nullptr);
    }
    static void dispatch(VkCommandBuffer cmd,const XeSSStage& stage,const Push& push) {
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,stage.pipeline);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,stage.layout,0,1,&stage.set,0,nullptr);
        vkCmdPushConstants(cmd,stage.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),&push);
        vkCmdDispatch(cmd,(push.width+7)/8,(push.height+7)/8,1);
    }
public:
    static void validate(const FfxVkXeSSReplayExtent& extent,uint32_t slots,uint32_t slot,
        const FfxVkXeSSRecoveredImages& images,const FfxVkXeSSRecoveredConversion& conversion) {
        require(slot<slots,"Invalid XeSS conversion slot");
        require(images.structSize==sizeof(images) && conversion.structSize==sizeof(conversion),"Unsupported conversion ABI");
        require(std::isfinite(conversion.colorScale) && conversion.colorScale>0 &&
            std::isfinite(conversion.outputScale) && conversion.outputScale>0 &&
            std::isfinite(conversion.motionScaleX) && std::isfinite(conversion.motionScaleY),"Invalid XeSS unit conversion");
        const FfxVkXeSSRecoveredImage external[]={images.color,images.motion,images.depth,images.output};
        for(unsigned i=0;i<4;++i) {
            const auto& image=external[i];
            require(image.image && image.view && image.extent.width==(i==3?extent.outputWidth:extent.inputWidth) &&
                image.extent.height==(i==3?extent.outputHeight:extent.inputHeight),"Unsupported conversion image extent");
            if(i<3) require(image.image!=images.output.image,"Conversion output must not alias input");
        }
        require(images.color.format==VK_FORMAT_R16G16B16A16_SFLOAT &&
            (images.motion.format==VK_FORMAT_R16G16_SFLOAT || images.motion.format==VK_FORMAT_R16G16B16A16_SFLOAT) &&
            images.depth.format==VK_FORMAT_R32_SFLOAT && images.output.format==VK_FORMAT_R16G16B16A16_SFLOAT,
            "Unsupported conversion image format");
    }
    XeSSInputConversion(VkPhysicalDevice physical,VkDevice device,FfxVkXeSSReplayExtent extent,uint32_t slots)
        :device_(device),extent_(extent),resources_(physical,device),slots_(slots) {
        const auto sampled=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,storage=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        std::vector<VkDescriptorSetLayoutBinding> pre={{0,sampled,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {1,sampled,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,storage,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {3,storage,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{4,sampled,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {5,storage,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
        std::vector<VkDescriptorSetLayoutBinding> post={{0,sampled,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {2,storage,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
        const auto before=resources_.createStage({std::begin(xess_input_conversion_spv),std::end(xess_input_conversion_spv)},pre,sizeof(Push));
        const auto after=resources_.createStage({std::begin(xess_output_conversion_spv),std::end(xess_output_conversion_spv)},post,sizeof(Push));
        for(uint32_t i=0;i<slots;++i) {
            auto& slot=slots_[i];slot.before=i?resources_.createBindings(before):before;
            slot.after=i?resources_.createBindings(after):after;
            slot.color=resources_.createImage(extent.inputWidth,extent.inputHeight,VK_FORMAT_R16G16B16A16_SFLOAT);
            slot.motion=resources_.createImage(extent.inputWidth,extent.inputHeight,VK_FORMAT_R16G16_SFLOAT);
            slot.depth=resources_.createImage(extent.inputWidth,extent.inputHeight,VK_FORMAT_R32_SFLOAT);
            slot.output=resources_.createImage(extent.outputWidth,extent.outputHeight,VK_FORMAT_R16G16B16A16_SFLOAT);
        }
    }
    XeSSFixedImages bind(uint32_t index,const FfxVkXeSSRecoveredImages& images,const FfxVkXeSSRecoveredConversion& conversion) {
        validate(extent_,static_cast<uint32_t>(slots_.size()),index,images,conversion);
        auto& slot=slots_[index];slot.conversion=conversion;
        bindImage(slot.before,0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,images.color.view);
        bindImage(slot.before,1,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,images.motion.view);
        bindImage(slot.before,2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,slot.color.view);
        bindImage(slot.before,3,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,slot.motion.view);
        bindImage(slot.before,4,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,images.depth.view);
        bindImage(slot.before,5,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,slot.depth.view);
        bindImage(slot.after,0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,slot.output.view);
        bindImage(slot.after,2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,images.output.view);
        return {{slot.color.handle,slot.color.view,{extent_.inputWidth,extent_.inputHeight},VK_FORMAT_R16G16B16A16_SFLOAT},
            {slot.motion.handle,slot.motion.view,{extent_.inputWidth,extent_.inputHeight},VK_FORMAT_R16G16_SFLOAT},
            {slot.depth.handle,slot.depth.view,{extent_.inputWidth,extent_.inputHeight},VK_FORMAT_R32_SFLOAT},
            {slot.output.handle,slot.output.view,{extent_.outputWidth,extent_.outputHeight},VK_FORMAT_R16G16B16A16_SFLOAT}};
    }
    void discardHistory() noexcept {for(auto& slot:slots_) slot.initialized=false;}
    void before(VkCommandBuffer cmd,uint32_t index) {
        auto& slot=slots_[index];
        if(!slot.initialized) {
            for(const auto& image:{slot.color,slot.motion,slot.depth,slot.output}) {
                VkImageMemoryBarrier b{};b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;b.image=image.handle;
                b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
                b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
                VkClearColorValue clear{};vkCmdClearColorImage(cmd,image.handle,VK_IMAGE_LAYOUT_GENERAL,&clear,1,&b.subresourceRange);
            }
            barrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
            slot.initialized=true;
        }
        barrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
        dispatch(cmd,slot.before,{extent_.inputWidth,extent_.inputHeight,slot.conversion.motionScaleX,
            slot.conversion.motionScaleY,slot.conversion.colorScale});
        barrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
    }
    void after(VkCommandBuffer cmd,uint32_t index) {
        const auto& slot=slots_[index];
        barrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
        dispatch(cmd,slot.after,{extent_.outputWidth,extent_.outputHeight,0,0,slot.conversion.outputScale});
    }
};
} // namespace ffxvk
