/*
Copyright (c) 2023 - 2023 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once

#include <assert.h>
#include <stdint.h>
#include <mutex>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <string.h>
#include <hip/hip_runtime.h>
#include "rocdecode.h"
#include "rocparser.h"
#include "commons.h"

#define MAX_FRAME_NUM     16

typedef enum{
    SEI_TYPE_TIME_CODE = 136,
    SEI_TYPE_USER_DATA_UNREGISTERED = 5
}SEI_H264_HEVC_PAYLOAD_TYPE;


#define ROCDEC_API_CALL( rocDecAPI )                                                                                 \
    do {                                                                                                            \
        rocDecStatus errorCode = rocDecAPI;                                                                             \
        if( errorCode != ROCDEC_SUCCESS) {                                                                          \
            std::ostringstream errorLog;                                                                           \
            errorLog << #rocDecAPI << " returned error " << errorCode;                                              \
            THROW(errorLog.str() + TOSTR(errorCode)); \
        }                                                                                                          \
    } while (0)

#define HIP_API_CALL( call )                                                                                                 \
    do {                                                                                                                          \
        hipError_t hip_status = call;                                                                                            \
        if (hip_status != hipSuccess) {                                                                                          \
            const char *szErrName = NULL;                                                                                        \
            szErrName = hipGetErrorName(hip_status);                                                                             \
            std::ostringstream errorLog;                                                                                         \
            errorLog << "hip API error " << szErrName ;                                                                  \
            THROW(errorLog.str());                   \
        }                                                                                                                        \
    }                                                                                                                            \
    while (0)


struct Rect {
    int l, t, r, b;
};

struct Dim {
    int w, h;
};

static inline int align(int value, int alignment) {
   return (value + alignment - 1) & ~(alignment - 1);
}

typedef struct DecFrameBuffer_ {
    uint8_t *frame_ptr;      /**< device memory pointer for the decoded frame */
    int64_t  pts;          // timestamp
} DecFrameBuffer;


typedef struct OutputSurfaceInfoType {
    uint32_t output_width;               /**< Output width of decoded surface*/
    uint32_t output_height;              /**< Output height of decoded surface*/
    uint32_t output_pitch;            /**< Output pitch in bytes of luma plane, chroma pitch can be inferred based on chromaFormat*/
    uint32_t bytes_per_pixel;            /**< Output BytesPerPixel of decoded image*/
    uint32_t bit_depth;                  /**< Output BitDepth of the image*/
    uint32_t num_chroma_planes;          /**< Output Chroma number of planes*/
    uint64_t output_surface_size_in_bytes; /**< Output Image Size in Bytes; including both luma and chroma planes*/ 
    rocDecVideoSurfaceFormat surface_format;      /**< Chroma format of the decoded image*/
} OutputSurfaceInfo;

class RocVideoDecoder {
    public:
      /**
       * @brief Construct a new Roc Video Decoder object
       * 
       * @param hip_ctx 
       * @param b_use_device_mem 
       * @param codec 
       * @param device_id 
       * @param b_low_latency 
       * @param device_frame_pitched 
       * @param p_crop_rect 
       * @param p_resize_dim 
       * @param extract_user_SEI_Message 
       * @param max_width 
       * @param max_height 
       * @param clk_rate 
       * @param force_zero_latency 
       */
        RocVideoDecoder(int device_id, bool b_use_device_mem, rocDecVideoCodec codec, bool b_low_latency, bool device_frame_pitched,
                          const Rect *p_crop_rect, const Dim *p_resize_dim, bool extract_user_SEI_Message, int max_width, int max_height,
                          uint32_t clk_rate,  bool force_zero_latency);
        ~RocVideoDecoder();
        
        /**
         * @brief Get the output frame width
         */
        uint32_t GetWidth() { assert(width_); return width_;}

        /**
        *  @brief  This function is used to get the actual decode width
        */
        int GetDecodeWidth() { assert(width_); return width_; }

        /**
         * @brief Get the output frame height
         */
        uint32_t GetHeight() { assert(height_); return height_; }

        /**
        *  @brief  This function is used to get the current chroma height.
        */
        int GetChromaHeight() { assert(chroma_height_); return chroma_height_; }

        /**
        *  @brief  This function is used to get the number of chroma planes.
        */
        int GetNumChromaPlanes() { assert(num_chroma_planes_); return num_chroma_planes_; }

        /**
        *   @brief  This function is used to get the current frame size based on pixel format.
        */
        int GetFrameSize() { assert(width_); return width_ * (height_ + (chroma_height_ * num_chroma_planes_)) * byte_per_pixel_; }

        /**
        *   @brief  This function is used to get the current frame size based on pitch
        */
        int GetFrameSizePitched() { assert(surface_stride_); return surface_stride_ * (height_ + (chroma_height_ * num_chroma_planes_)); }

        /**
         * @brief Get the Bit Depth and BytesPerPixel associated with the pixel format
         * 
         * @return uint32_t 
         */
        uint32_t GetBitDepth() { assert(bitdepth_minus_8_); return (bitdepth_minus_8_ + 8); }
        uint32_t GetBytePerPixel() { assert(byte_per_pixel_); return byte_per_pixel_; }
        /**
         * @brief Functions to get the output surface attributes
         */
        size_t GetSurfaceSize() { assert(surface_size_); return surface_size_; }
        uint32_t GetSurfaceStride() { assert(surface_stride_); return surface_stride_; }
        //RocDecImageFormat GetSubsampling() { return subsampling_; }
        int GetSurfaceWidth() { assert(surface_width_); return surface_width_;}
        int GetSurfaceHeight() { assert(surface_height_); return surface_height_;}
        /**
         * @brief Get the name of the output format
         * 
         * @param codec_id 
         * @return std::string 
         */
        const char *GetCodecFmtName(rocDecVideoCodec codec_id);
        /**
         * @brief Get the pointer to the Output Image Info 
         * 
         * @param surface_info ptr to output surface info 
         * @return true 
         * @return false 
         */
        bool GetOutputSurfaceInfo(OutputSurfaceInfo **surface_info);
        /**
         * @brief this function decodes a frame and returns the number of frames avalable for display
         * 
         * @param data - pointer to the data buffer that is to be decode
         * @param size - size of the data buffer in bytes
         * @param pts - presentation timestamp
         * @param flags - video packet flags
         * @return int - num of frames to display
         */
        int DecodeFrame(const uint8_t *data, size_t size, int pkt_flags, int64_t pts = 0);
        /**
         * @brief This function returns a decoded frame and timestamp. This should be called in a loop fetching all the available frames
         * 
         */
        uint8_t* GetFrame(int64_t *pts);

        /**
         * @brief utility function to save image to a file
         * 
         * @param output_file_name - file to write
         * @param dev_mem - dev_memory pointer of the frame
         * @param image_info - output image info
         * @param is_output_RGB - to write in RGB
         */
        //void SaveImage(std::string output_file_name, void* dev_mem, OutputImageInfo* image_info, bool is_output_RGB = 0);

        /**
         * @brief Get the Device info for the current device
         * 
         * @param device_name 
         * @param gcn_arch_name 
         * @param pci_bus_id 
         * @param pci_domain_id 
         * @param pci_device_id 
         */
        void GetDeviceinfo(std::string &device_name, std::string &gcn_arch_name, int &pci_bus_id, int &pci_domain_id, int &pci_device_id);

    private:
        int decoder_session_id_; // Decoder session identifier. Used to gather session level stats.
        /**
         *   @brief  Callback function to be registered for getting a callback when decoding of sequence starts
         */
        static int ROCDECAPI HandleVideoSequenceProc(void *pUserData, RocdecVideoFormat *pVideoFormat) { return ((RocVideoDecoder *)pUserData)->HandleVideoSequence(pVideoFormat); }

        /**
         *   @brief  Callback function to be registered for getting a callback when a decoded frame is ready to be decoded
         */
        static int ROCDECAPI HandlePictureDecodeProc(void *pUserData, RocdecPicParams *pPicParams) { return ((RocVideoDecoder *)pUserData)->HandlePictureDecode(pPicParams); }

        /**
         *   @brief  Callback function to be registered for getting a callback when a decoded frame is available for display
         */
        static int ROCDECAPI HandlePictureDisplayProc(void *pUserData, RocdecParserDispInfo *pDispInfo) { return ((RocVideoDecoder *)pUserData)->HandlePictureDisplay(pDispInfo); }

        /**
         *   @brief  Callback function to be registered for getting a callback when all the unregistered user SEI Messages are parsed for a frame.
         */
        static int ROCDECAPI HandleSEIMessagesProc(void *pUserData, RocdecSeiMessageInfo *pSEIMessageInfo) { return ((RocVideoDecoder *)pUserData)->GetSEIMessage(pSEIMessageInfo); } 

        /**
         *   @brief  This function gets called when a sequence is ready to be decoded. The function also gets called
             when there is format change
        */
        int HandleVideoSequence(RocdecVideoFormat *pVideoFormat);

        /**
         *   @brief  This function gets called when a picture is ready to be decoded. cuvidDecodePicture is called from this function
         *   to decode the picture
         */
        int HandlePictureDecode(RocdecPicParams *pPicParams);

        /**
         *   @brief  This function gets called after a picture is decoded and available for display. Frames are fetched and stored in 
             internal buffer
        */
        int HandlePictureDisplay(RocdecParserDispInfo *pDispInfo);
        /**
         *   @brief  This function gets called when all unregistered user SEI messages are parsed for a frame
         */
        int GetSEIMessage(RocdecSeiMessageInfo *pSEIMessageInfo);

        /**
         *   @brief  This function reconfigure decoder if there is a change in sequence params.
         */
        int ReconfigureDecoder(RocdecVideoFormat *pVideoFormat);

        /**
         * @brief Function to Initialize GPU-HIP
         * 
         */
        bool InitHIP(int device_id);
        int num_devices_;
        int device_id_;
        RocdecVideoParser rocdec_parser_ = nullptr;
        rocDecDecoderHandle roc_decoder_ = nullptr;
        bool b_use_device_mem_ = true;
        bool b_extract_sei_message_ = false;
        bool b_low_latency_ = true;
        bool b_force_zero_latency_ = true;
        bool b_device_frame_pitched_ = true;
        hipDeviceProp_t hip_dev_prop_;
        hipStream_t hip_stream_;
        rocDecVideoCodec codec_id_ = rocDecVideoCodec_NumCodecs;
        rocDecVideoChromaFormat video_chroma_format_ = rocDecVideoChromaFormat_420;
        rocDecVideoSurfaceFormat video_surface_format_ = rocDecVideoSurfaceFormat_NV12;
        RocdecVideoFormat video_format_ = {};
        RocdecSeiMessageInfo *curr_sei_message_ptr_ = nullptr;
        RocdecSeiMessageInfo sei_message_display_q_[MAX_FRAME_NUM];
        int decoded_frame_cnt_ = 0, decoded_frame_cnt_ret_ = 0;
        int decode_poc_ = 0, pic_num_in_dec_order_[MAX_FRAME_NUM];
        int num_alloced_frames_ = 0;
        std::ostringstream input_video_info_str_;
        int bitdepth_minus_8_ = 0;
        uint32_t byte_per_pixel_ = 1;
        uint32_t width_;
        uint32_t height_;
        int max_width_, max_height_;
        uint32_t chroma_height_;
        uint32_t surface_height_;
        uint32_t surface_width_;
        uint32_t num_chroma_planes_;
        uint32_t num_components_;
        uint32_t surface_stride_;
        size_t surface_size_;
        OutputSurfaceInfo output_surface_info_;
        std::mutex mtx_vp_frame_;
        std::vector<DecFrameBuffer> vp_frames_;      // vector of decoded frames
        Rect disp_rect_ = {};
        Rect crop_rect_ = {};
        Dim resize_dim_ = {};
        FILE *fp_sei_ = NULL;
        FILE *fp_out_ = NULL;
};