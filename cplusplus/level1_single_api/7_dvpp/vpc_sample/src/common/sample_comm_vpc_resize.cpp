/**
 *  Copyright [2021] Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "sample_comm.h"

using namespace std;

int32_t sample_comm_vpc_resize(FuncInput funcInput)
{
    if (g_run_mode == ACL_HOST) {
        aclError aclRet = aclrtSetCurrentContext(g_context);
        if (aclRet != ACL_SUCCESS) {
            SAMPLE_PRT("set current context failed, ret = %d\n", aclRet);
            return HI_FAILURE;
        }
    }

    char inputFileName[FILE_NAME_LEN];
    strcpy(inputFileName, funcInput.g_vpc_attribute.inputFileName);
    char outputFileName[FILE_NAME_LEN];
    strcpy(outputFileName, funcInput.g_vpc_attribute.outputFileName);
    uint32_t width = funcInput.g_vpc_attribute.width;
    uint32_t height = funcInput.g_vpc_attribute.height;
    uint32_t format = funcInput.g_vpc_attribute.format;
    uint32_t outWidth = funcInput.g_vpc_attribute.outWidth;
    uint32_t outHeight = funcInput.g_vpc_attribute.outHeight;
    uint32_t outFormat = funcInput.g_vpc_attribute.outFormat;
    double fx = funcInput.g_vpc_attribute.fx;
    double fy = funcInput.g_vpc_attribute.fy;
    int32_t interpolation = funcInput.g_vpc_attribute.interpolation;
    hi_vpc_chn chnId = funcInput.chnId;

    // configure the input picture
    hi_vpc_pic_info inputPic;
    inputPic.picture_width = width;
    inputPic.picture_height = height;
    inputPic.picture_format = static_cast<hi_pixel_format>(format);
    configure_stride_and_buffer_size(inputPic);

    // configure the output picture
    hi_vpc_pic_info outputPic;
    outputPic.picture_width = outWidth;
    outputPic.picture_height = outHeight;
    // if outWidth and outHeight are set to 0, fx and fy must be configured
    if (outWidth == 0 || outHeight == 0) {
        outputPic.picture_width = static_cast<uint32_t>(width * fx + 0.5); // 0.5 is for rounding
        outputPic.picture_height = static_cast<uint32_t>(height * fy + 0.5); // 0.5 is for rounding
    }
    outputPic.picture_format = static_cast<hi_pixel_format>(outFormat);
    uint32_t dstBufferSize = configure_stride_and_buffer_size(outputPic);

    int32_t ret = prepare_input_data(inputPic, inputFileName);
    if (ret != HI_SUCCESS) {
        SAMPLE_PRT("prepare input data failed!\n");
        return ret;
    }

    // malloc output buffer
    ret = dvpp_mem_malloc(&outputPic.picture_address, outputPic.picture_buffer_size);
    if (ret != HI_SUCCESS) {
        SAMPLE_PRT("output buffer alloc failed!\n");
        dvpp_mem_free(inputPic.picture_address);
        inputPic.picture_address = nullptr;
        return HI_FAILURE;
    }
    memset_buffer(outputPic);

    // start to call hi_mpi_vpc_resize interface
    uint32_t taskID = 0;
    ret = hi_mpi_vpc_resize(chnId, &inputPic, &outputPic, fx, fy, interpolation, &taskID, -1);
    if (ret != HI_SUCCESS) {
        SAMPLE_PRT("hi_mpi_vpc_resize failed, ret = %#x!\n", ret);
        dvpp_mem_free(inputPic.picture_address);
        inputPic.picture_address = nullptr;
        dvpp_mem_free(outputPic.picture_address);
        outputPic.picture_address = nullptr;
        return HI_FAILURE;
    }

    // asign the taskID that you get from the last interface to get the process result
    uint32_t taskIDResult = taskID;
    ret = hi_mpi_vpc_get_process_result(chnId, taskIDResult, -1);
    if (ret != HI_SUCCESS) {
        SAMPLE_PRT("hi_mpi_vpc_get_process_result failed, ret = %#x!\n", ret);
        dvpp_mem_free(inputPic.picture_address);
        inputPic.picture_address = nullptr;
        dvpp_mem_free(outputPic.picture_address);
        outputPic.picture_address = nullptr;
        return HI_FAILURE;
    }

    ret = handle_output_data(outputPic, dstBufferSize, outputFileName);
    if (ret != HI_SUCCESS) {
        SAMPLE_PRT("handle_output_data failed!\n");
    }

    // free input and output buffer
    dvpp_mem_free(inputPic.picture_address);
    inputPic.picture_address = nullptr;
    dvpp_mem_free(outputPic.picture_address);
    outputPic.picture_address = nullptr;
    return ret;
}