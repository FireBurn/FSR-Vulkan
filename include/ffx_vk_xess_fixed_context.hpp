// SPDX-License-Identifier: MIT
// Experimental recovered XeSS 2.0.2 cross-vendor graph, fixed quality profile.
#pragma once
#include "ffx_vk_xess_resources.hpp"
#include "ffx_vk_xess_convolution_graph.h"
#include "ffx_vk_xess_replay_constants.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace ffxvk {
struct XeSSConvolutionModel {
    std::vector<uint32_t> shader;
    std::array<std::vector<uint8_t>,3> buffers;
};
struct XeSSFixedModel {
    std::vector<uint32_t> prepareShader, resolveShader;
    std::array<XeSSConvolutionModel,12> convolutions;
    std::array<std::vector<uint8_t>,3> resolveBuffers;
};
struct XeSSExternalImage {
    VkImage image{};
    VkImageView view{};
    VkExtent2D extent{};
    VkFormat format=VK_FORMAT_UNDEFINED;
};
struct XeSSFixedImages {
    XeSSExternalImage color,motion,depth,output;
};

struct XeSSFixedGeometry {
    std::array<std::array<uint32_t,3>,13> tensors{};
    std::array<std::array<uint32_t,3>,12> groups{};
    std::array<uint32_t,3> prepare{},resolve{};
    explicit XeSSFixedGeometry(const FfxVkXeSSReplayExtent& extent) {
        if(!ffxVkXeSSReplayExtentSupported(&extent)) throw std::invalid_argument("Unverified XeSS extent");
        auto ceilDiv=[](uint32_t n,uint32_t d){return (n+d-1)/d;};
        tensors[0]={ceilDiv(extent.outputWidth,2),ceilDiv(extent.outputHeight,2),1};
        for(unsigned i=1;i<5;++i)
            tensors[i]={ceilDiv(tensors[i-1][0],2),ceilDiv(tensors[i-1][1],2),i==1?1u:1u<<(i-1)};
        tensors[5]={tensors[4][0],tensors[4][1],4};
        tensors[6]={tensors[3][0],tensors[3][1],4};
        tensors[7]={tensors[3][0],tensors[3][1],2};
        tensors[8]={tensors[2][0],tensors[2][1],2};
        tensors[9]={tensors[2][0],tensors[2][1],1};
        tensors[10]={tensors[1][0],tensors[1][1],1};tensors[11]=tensors[10];tensors[12]=tensors[0];
        constexpr uint32_t tiles[12][2]={{32,32},{8,32},{8,16},{16,8},{16,16},{8,16},
            {16,8},{32,8},{8,8},{16,32},{8,8},{16,32}};
        for(unsigned i=0;i<12;++i) {
            auto size=tensors[i<5?i:i+1];
            groups[i]={ceilDiv(size[0],tiles[i][0]),ceilDiv(size[1],tiles[i][1]),1};
        }
        prepare={ceilDiv(extent.outputWidth,16),ceilDiv(extent.outputHeight,32),1};
        resolve={ceilDiv(extent.outputWidth,8),ceilDiv(extent.outputHeight,8),1};
    }
};

// The engine owns the device, external images, commands and queue scheduling.
// It enables Vulkan 1.3, runtimeDescriptorArray, bufferDeviceAddress, shaderInt8,
// uniformBufferStandardLayout, shaderIntegerDotProduct, formatless storage
// writes, and dynamic sampled/storage image, storage/uniform buffer indexing.
// Limits: >=32 storage images, 24 sampled images, 24 uniform buffers and six
// storage buffers per stage. External views must be single-layer, mip-0 2D;
// inputs require SAMPLED usage, output STORAGE usage, and GENERAL layouts.
// Motion is previous-minus-current in input pixels, device depth is [0,1],
// colour is linear HDR with exposure 1. Origins and quality are fixed.
//
// Retire a frame slot's prior use before rebinding it; other slots may remain
// queued. Retire all uses before destruction. Submit recorded frames once,
// in order, on the same queue: internal temporal history is shared. The caller
// makes external producer writes visible to compute reads/writes and adds
// consumer-stage barriers for output. No submission, wait, readback or file I/O
// happens here. If a recorded frame is discarded, retire prior submissions,
// call discardHistory(), and start again with reset=1. Internal layouts and
// history are then reinitialized. Model bytes may be released after creation.
class XeSSFixedContext final {
    VkDevice device_;
    FfxVkXeSSReplayExtent extent_;
    XeSSFixedGeometry geometry_;
    XeSSResources resources_;
    struct Tensor {XeSSImage image;uint32_t layers;};
    std::vector<Tensor> internal_;
    XeSSImage history_,mask_,prepared_;
    std::array<XeSSImage,13> tensors_{};
    XeSSStage prepare_{},resolve_;
    std::vector<XeSSStage> prepareBindings_,resolveBindings_;
    std::array<XeSSStage,12> convolutions_{};
    std::array<FfxVkXeSSConvolutionPass,12> graph_{};
    VkSampler sampler_{};
    std::vector<uint8_t> bound_;
    bool initialized_=false;

    static void require(bool condition,const char* message) {
        if(!condition) throw std::invalid_argument(message);
    }
    static void codeValid(const std::vector<uint32_t>& code) {
        require(code.size()>=5 && code[0]==0x07230203u,"Invalid XeSS shader header");
    }
    XeSSImage image(uint32_t w,uint32_t h,VkFormat format,bool array=false,uint32_t layers=1) {
        auto result=resources_.createImage(w,h,format,array,layers);
        internal_.push_back({result,layers});return result;
    }
    XeSSStage stage(const std::vector<uint32_t>& code,
        const std::vector<VkDescriptorType>& types,const std::vector<uint32_t>& counts,uint32_t bytes) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for(uint32_t i=0;i<types.size();++i)
            bindings.push_back({i,types[i],counts[i],VK_SHADER_STAGE_COMPUTE_BIT,nullptr});
        return resources_.createStage(code,bindings,bytes);
    }
    void bindImage(const XeSSStage& stage,uint32_t binding,uint32_t slot,
        VkDescriptorType type,VkImageView view) {
        VkDescriptorImageInfo info{sampler_,view,VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet write{};write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet=stage.set;write.dstBinding=binding;write.dstArrayElement=slot;
        write.descriptorCount=1;write.descriptorType=type;write.pImageInfo=&info;
        vkUpdateDescriptorSets(device_,1,&write,0,nullptr);
    }
    void bindBuffer(const XeSSStage& stage,uint32_t binding,uint32_t slot,
        VkDescriptorType type,XeSSBuffer buffer,size_t bytes) {
        VkDescriptorBufferInfo info{buffer.handle,0,bytes};
        VkWriteDescriptorSet write{};write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet=stage.set;write.dstBinding=binding;write.dstArrayElement=slot;
        write.descriptorCount=1;write.descriptorType=type;write.pBufferInfo=&info;
        vkUpdateDescriptorSets(device_,1,&write,0,nullptr);
    }
    static void barrier(VkCommandBuffer cmd,VkPipelineStageFlags source,
        VkPipelineStageFlags destination,VkAccessFlags from,VkAccessFlags to) {
        VkMemoryBarrier b{};b.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        b.srcAccessMask=from;b.dstAccessMask=to;
        vkCmdPipelineBarrier(cmd,source,destination,0,1,&b,0,nullptr,0,nullptr);
    }
    static void dispatch(VkCommandBuffer cmd,const XeSSStage& stage,
        const uint32_t* constants,uint32_t bytes,uint32_t x,uint32_t y) {
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,stage.pipeline);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,stage.layout,0,1,&stage.set,0,nullptr);
        vkCmdPushConstants(cmd,stage.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,bytes,constants);
        vkCmdDispatch(cmd,x,y,1);
    }
public:
    // Default 720p capture tables retained for bundle adapter compatibility.
    // Runtime dimensions/groups come from XeSSFixedGeometry for the extent.
    inline static constexpr std::array<std::array<uint32_t,3>,13> tensorShapes={{
        {640,360,1},{320,180,1},{160,90,2},{80,45,4},{40,23,8},{40,23,4},
        {80,45,4},{80,45,2},{160,90,2},{160,90,1},{320,180,1},{320,180,1},{640,360,1}}};
    inline static constexpr std::array<uint32_t,12> skipInputs={0,1,2,3,4,3,6,2,8,1,10,0};
    inline static constexpr std::array<std::array<uint32_t,3>,12> dispatchGroups={{
        {20,12,1},{40,6,1},{20,6,1},{5,6,1},{3,2,1},{10,3,1},
        {5,6,1},{5,12,1},{20,12,1},{20,6,1},{40,23,1},{40,12,1}}};

    XeSSFixedContext(VkPhysicalDevice physical,VkDevice device,const XeSSFixedModel& model,
        FfxVkXeSSReplayExtent extent={753,424,1280,720},uint32_t frameSlots=1)
        :device_(device),extent_(extent),geometry_(extent),resources_(physical,device) {
        // Validate the entire profile before creating Vulkan objects.
        require(frameSlots>=1 && frameSlots<=8,"XeSS frame slots must be in [1,8]");
        bound_.resize(frameSlots,0);
        codeValid(model.prepareShader);codeValid(model.resolveShader);
        constexpr size_t weightBytes[]={2304,4608,18432,73728,73728,36864,18432,9216,4608,2304,2304,2304};
        constexpr size_t channels[]={16,32,64,128,64,64,32,32,16,16,16,16};
        constexpr size_t resolveBytes[]={512,256,256};
        for(size_t i=0;i<12;++i) {
            const auto& pass=model.convolutions[i];codeValid(pass.shader);
            require(pass.buffers[0].size()==weightBytes[i] &&
                pass.buffers[1].size()==channels[i]*4 && pass.buffers[2].size()==channels[i]*4,
                "Unsupported XeSS convolution weights");
        }
        for(unsigned i=0;i<3;++i) require(model.resolveBuffers[i].size()==resolveBytes[i],"Unsupported XeSS resolve weights");
        history_=image(extent.outputWidth,extent.outputHeight,VK_FORMAT_R16G16B16A16_SFLOAT);
        mask_=image(extent.outputWidth,extent.outputHeight,VK_FORMAT_R8_UNORM);
        prepared_=image(extent.outputWidth,extent.outputHeight,VK_FORMAT_R16G16B16A16_SFLOAT);
        for(size_t i=0;i<tensorShapes.size();++i) {
            auto shape=geometry_.tensors[i];tensors_[i]=image(shape[0],shape[1],VK_FORMAT_R32G32B32A32_UINT,true,shape[2]);
        }
        const auto sampled=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,storage=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        const auto ssbo=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,ubo=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        prepare_=stage(model.prepareShader,{sampled,storage,storage,VK_DESCRIPTOR_TYPE_SAMPLER},{16,16,16,1},120);
        resolve_=stage(model.resolveShader,{sampled,sampled,storage,ubo,ubo},{12,12,12,12,12},92);
        prepareBindings_.push_back(prepare_);resolveBindings_.push_back(resolve_);
        for(uint32_t i=1;i<frameSlots;++i) {
            prepareBindings_.push_back(resources_.createBindings(prepare_));
            resolveBindings_.push_back(resources_.createBindings(resolve_));
        }
        VkSamplerCreateInfo si{};si.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter=si.minFilter=VK_FILTER_LINEAR;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.addressModeU=si.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;si.maxLod=VK_LOD_CLAMP_NONE;
        sampler_=resources_.createSampler(si);
        for(unsigned i=0;i<12;++i) {
            const auto& spec=model.convolutions[i];auto s=stage(spec.shader,{sampled,ssbo,storage},{6,6,6},24);
            convolutions_[i]=s;graph_[i].pipeline=s.pipeline;graph_[i].descriptors=s.set;
            const auto shape=geometry_.tensors[i<5?i:i+1];
            graph_[i].constants[0]=shape[0];graph_[i].constants[1]=shape[1];
            // Captured root-address words 2/3 are unused by this normalized
            // profile. Keep them zero; descriptor-table bases are 0 and 3.
            graph_[i].constants[5]=3;
            std::copy(geometry_.groups[i].begin(),geometry_.groups[i].end(),graph_[i].groups);
            std::array<XeSSBuffer,3> buffers;
            for(unsigned j=0;j<3;++j) {
                buffers[j]=resources_.createBuffer(spec.buffers[j].size(),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                std::memcpy(buffers[j].mapped,spec.buffers[j].data(),spec.buffers[j].size());
            }
            for(unsigned slot=0;slot<6;++slot) {
                bindImage(s,0,slot,sampled,tensors_[slot==1?skipInputs[i]:i].view);
                bindImage(s,2,slot,storage,tensors_[i+1].view);
                unsigned index=slot>=3?slot-3:0;
                bindBuffer(s,1,slot,ssbo,buffers[index],spec.buffers[index].size());
            }
        }
        for(unsigned j=0;j<3;++j) {
            auto buffer=resources_.createBuffer(model.resolveBuffers[j].size(),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            std::memcpy(buffer.mapped,model.resolveBuffers[j].data(),model.resolveBuffers[j].size());
            // Slots 9..11 select the three model CBVs; earlier slots alias 0.
            for(const auto& resolve:resolveBindings_)
                for(unsigned binding=3;binding<5;++binding)
                    for(unsigned slot=0;slot<12;++slot)
                        if((slot>=9?slot-9:0)==j) bindBuffer(resolve,binding,slot,ubo,buffer,model.resolveBuffers[j].size());
        }
    }
    XeSSFixedContext(const XeSSFixedContext&)=delete;
    XeSSFixedContext& operator=(const XeSSFixedContext&)=delete;

    uint32_t frameSlotCount() const noexcept {return static_cast<uint32_t>(bound_.size());}
    // Retire this slot's prior use first. Binding validation completes before descriptors
    // are modified. No ownership of these images or views is transferred.
    void bind(const XeSSFixedImages& images,uint32_t frameSlot=0) {
        require(frameSlot<bound_.size(),"Invalid XeSS frame slot");
        const auto& prepare=prepareBindings_[frameSlot];const auto& resolve=resolveBindings_[frameSlot];
        std::array<XeSSExternalImage,4> external={images.color,images.motion,images.depth,images.output};
        const VkFormat formats[]={VK_FORMAT_R16G16B16A16_SFLOAT,VK_FORMAT_R16G16_SFLOAT,VK_FORMAT_R32_SFLOAT,VK_FORMAT_R16G16B16A16_SFLOAT};
        for(unsigned i=0;i<4;++i) {
            const auto& im=external[i];
            require(im.image && im.view && im.format==formats[i] && im.extent.width==(i==3?extent_.outputWidth:extent_.inputWidth) &&
                im.extent.height==(i==3?extent_.outputHeight:extent_.inputHeight),"Unsupported XeSS external image");
            if(i<3) require(im.image!=images.output.image,"XeSS output must not alias input");
        }
        const auto sampled=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,storage=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        for(unsigned binding=0;binding<4;++binding) for(unsigned slot=0;slot<(binding==3?1u:16u);++slot) {
            VkImageView view=images.color.view;
            if(binding==0) {
                if(slot==1) view=images.motion.view;
                if(slot==3) view=images.depth.view;
                if(slot==8) view=history_.view;
                if(slot==9) view=mask_.view;
            } else if(binding==1) view=slot==2?images.output.view:prepared_.view;
            else if(binding==2) view=tensors_[0].view;
            bindImage(prepare,binding,slot,binding==0?sampled:binding==3?VK_DESCRIPTOR_TYPE_SAMPLER:storage,view);
        }
        for(unsigned binding=0;binding<3;++binding) for(unsigned slot=0;slot<12;++slot) {
            VkImageView view=images.color.view;
            if(binding==0) {if(slot==1)view=images.motion.view;if(slot==3)view=images.depth.view;if(slot==5)view=prepared_.view;}
            else if(binding==1) view=tensors_[12].view;
            else view=slot==7?history_.view:slot==8?mask_.view:images.output.view;
            bindImage(resolve,binding,slot,binding==2?storage:sampled,view);
        }
        bound_[frameSlot]=1;
    }
    void discardHistory() noexcept {initialized_=false;}

    VkResult validateRecord(VkCommandBuffer cmd,const FfxVkXeSSReplayFrame& frame,uint32_t frameSlot=0) const noexcept {
        uint32_t preparePush[30],resolvePush[23];
        if(!cmd || frameSlot>=bound_.size() || !bound_[frameSlot] || (!initialized_ && !frame.reset) ||
            !ffxVkXeSSReplayConstantsForExtent(&extent_,&frame,preparePush,resolvePush)) return VK_ERROR_INITIALIZATION_FAILED;
        return VK_SUCCESS;
    }
    VkResult record(VkCommandBuffer cmd,const FfxVkXeSSReplayFrame& frame,uint32_t frameSlot=0) {
        const auto valid=validateRecord(cmd,frame,frameSlot);
        if(valid!=VK_SUCCESS) return valid;
        uint32_t preparePush[30],resolvePush[23];
        ffxVkXeSSReplayConstantsForExtent(&extent_,&frame,preparePush,resolvePush);
        if(!initialized_) {
            for(const auto& tensor:internal_) {
                VkImageMemoryBarrier b{};b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                b.image=tensor.image.handle;b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
                b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,tensor.layers};b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
                VkClearColorValue clear{};vkCmdClearColorImage(cmd,b.image,VK_IMAGE_LAYOUT_GENERAL,&clear,1,&b.subresourceRange);
            }
            barrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
        }
        barrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
        dispatch(cmd,prepareBindings_[frameSlot],preparePush,120,geometry_.prepare[0],geometry_.prepare[1]);
        barrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
        const auto result=ffxVkXeSSRecordConvolutions(cmd,convolutions_[0].layout,graph_.data(),12);
        if(result!=VK_SUCCESS) return result; // Constructor validates every pass.
        dispatch(cmd,resolveBindings_[frameSlot],resolvePush,92,geometry_.resolve[0],geometry_.resolve[1]);
        initialized_=true;return VK_SUCCESS;
    }
};
} // namespace ffxvk
