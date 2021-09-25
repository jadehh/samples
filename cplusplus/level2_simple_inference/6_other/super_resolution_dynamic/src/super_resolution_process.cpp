/**

* Copyright 2020 Huawei Technologies Co., Ltd

*

* Licensed under the Apache License, Version 2.0 (the "License");

* you may not use this file except in compliance with the License.

* You may obtain a copy of the License at



* http://www.apache.org/licenses/LICENSE-2.0



* Unless required by applicable law or agreed to in writing, software

* distributed under the License is distributed on an "AS IS" BASIS,

* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

* See the License for the specific language governing permissions and

* limitations under the License.



* File sample_process.cpp

* Description: handle acl resource

*/

#include "super_resolution_process.h"
#include <iostream>
#include "acl/acl.h"
#include "model_process.h"
#include "utils.h"
using namespace std;

namespace {
    
    const string DYNAMIC_LIST[] = {"256x256",   // 0
                                   "512x512",  // 1
                                   "288x288"};    // 2
    
}

SuperResolutionProcess::SuperResolutionProcess(char **argv)
:deviceId_(0), context_(nullptr), stream_(nullptr), inputBuf_(nullptr), 
isInited_(false){
    modelWidth_ = atoi(argv[2]);
    modelHeight_ = atoi(argv[3]);
    if(256 == atoi(argv[2])){
        modelType_ = 0; 
    }else if(512 == atoi(argv[2])){
        modelType_ = 1; 
    }else
    {
        modelType_ = 2;
    }
    upScale_ = 3;
}

SuperResolutionProcess::~SuperResolutionProcess() {
    destroy_resource();
}

Result SuperResolutionProcess::init_resource() {
    // ACL init
    const char *aclConfigPath = "../src/acl.json";
    aclError ret = aclInit(aclConfigPath);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("Acl init failed");
        return FAILED;
    }
    INFO_LOG("Acl init success");
    // open device
    ret = aclrtSetDevice(deviceId_);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("Acl open device %d failed", deviceId_);
        return FAILED;
    }
    INFO_LOG("Open device %d success", deviceId_);
    ret = aclrtGetRunMode(&runMode_);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("acl get run mode failed");
        return FAILED;
    }
    return SUCCESS;
}

Result SuperResolutionProcess::init_model(const char* omModelPath) {
    Result ret = model_.load_model_from_file_with_mem(omModelPath);
    if (ret != SUCCESS) {
        ERROR_LOG("execute LoadModelFromFileWithMem failed");
        return FAILED;
    }
   
    return SUCCESS;

}

void SuperResolutionProcess::destroy_model() {
    model_.unload();
    model_.destroy_desc();
    model_.destroy_input();
    model_.destroy_output();
}

Result SuperResolutionProcess::init() {
    if (isInited_) {
        INFO_LOG("Classify instance is initied already!");
        return SUCCESS;
    }
    Result ret = init_resource();
    if (ret != SUCCESS) {
        ERROR_LOG("Init acl resource failed");
        return FAILED;
    }
    isInited_ = true;
    return SUCCESS;
}

Result SuperResolutionProcess::preprocess(const string& imageFile) {
    // read image using OPENCV
    cv::Mat mat = cv::imread(imageFile, CV_LOAD_IMAGE_GRAYSCALE);
    cv::resize(mat, mat,cv::Size(modelWidth_,modelHeight_));
    if (mat.empty()) {
        return FAILED;
    }
  
    outputWidth_ = modelWidth_ * upScale_;
    outputHeight_ =  modelHeight_ * upScale_;

    // prepreocess
    mat.convertTo(mat, CV_32FC1);
    mat = 1.0 * mat / 255;
    inputDataSize_ = RGBF32_CHAN_SIZE(modelWidth_, modelHeight_);
  
    // init model
    string modelPath = "../model/FSRCNN_x3.om";
    modelPath_ = modelPath.c_str();
    Result ret = init_model(modelPath_);

    if (ret != SUCCESS) {
        ERROR_LOG("execute LoadModelFromFileWithMem failed");
        return FAILED;
    }

    ret = model_.create_desc();
    if (ret != SUCCESS) {
        ERROR_LOG("execute CreateDesc failed");
        return FAILED;
    }

    aclrtMalloc(&inputBuf_, (size_t)(inputDataSize_), ACL_MEM_MALLOC_HUGE_FIRST);
  
    if (runMode_ == ACL_HOST) { 
        aclError ret = aclrtMemcpy(inputBuf_, inputDataSize_,
                                   mat.ptr<uint8_t>(), inputDataSize_,
                                   ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != ACL_ERROR_NONE) {
            ERROR_LOG("Copy resized image data to device failed.");
            return FAILED;
        }

    } else {
        memcpy(inputBuf_, mat.ptr<uint8_t>(), inputDataSize_);
    }

    ret = model_.create_input(inputBuf_, inputDataSize_);
    if (ret != SUCCESS) {
        ERROR_LOG("Create mode input dataset failed");
        return FAILED;
    }
    ret = model_.create_output();
    if (ret != SUCCESS) {
        ERROR_LOG("execute CreateOutput failed");
        return FAILED;
    }
    model_.set_dynamic_hw_size(modelWidth_, modelHeight_);
    return SUCCESS;

}

Result SuperResolutionProcess::inference(aclmdlDataset*& inferenceOutput) {
    Result ret = model_.execute();
    if (ret != SUCCESS) {
        ERROR_LOG("Execute model inference failed");
        return FAILED;
    }
    inferenceOutput = model_.get_model_output_data();
    return SUCCESS;
}

Result SuperResolutionProcess::postprocess(const string& imageFile, aclmdlDataset* modelOutput)
{
    uint32_t dataSize = 0;
    void* data = get_inference_output_item(dataSize, modelOutput);
    if (data == nullptr){
        return FAILED;
    }
    // get result data
    //data 指向用户数据的指针。矩阵构造器将会取数据和步长参数不分配矩阵数据。取而代之的是，仅仅初始化矩阵的头去指向具体的数据，这意味数据不会被拷贝。这个操作将会非常高效，并且可以用opencv的函数来处理外部的数据。值得注意的是，这些外部的数据不会被自动释放。
    //指定类型和大小的二维数组，具有预先存在的数据
    cv::Mat mat_y(outputHeight_, outputWidth_, CV_32FC1, const_cast<float*>((float*)data));
    mat_y.convertTo(mat_y, CV_8U, 255); // SRCNN, FSRCNN, ESPCN, VDSR
 
    // original image
    cv::Mat mat = cv::imread(imageFile, CV_LOAD_IMAGE_COLOR);
   // cv::resize(mat, mat,cv::Size(512,512));
    cv::resize(mat, mat,cv::Size(modelWidth_,modelHeight_));
    // bicubic
    cv::Mat mat_bicubic; 
    cv::resize(mat, mat_bicubic, cv::Size(), upScale_, upScale_, cv::INTER_CUBIC);
    // BGR2YCrCb and bicubic
    cv::cvtColor(mat, mat, cv::COLOR_BGR2YCrCb);
    cv::Mat resultImage; 
    cv::resize(mat, resultImage, cv::Size(), upScale_, upScale_, cv::INTER_CUBIC);
    // replace Y channel
    std::vector<cv::Mat> channels;
    cv::split(resultImage, channels);
   // cout << channels[0]<<endl;
    channels[0] = mat_y;
    cv::merge(channels, resultImage);
    // YCrCb2BGR
    cv::cvtColor(resultImage, resultImage, cv::COLOR_YCrCb2BGR);
    save_image(imageFile, mat_bicubic, resultImage);

    if (runMode_ == ACL_HOST) {
        delete[]((uint8_t *)data);
        data = nullptr;
    }
    return SUCCESS;
}

void SuperResolutionProcess::save_image(const string& origImageFile,
                                       cv::Mat& imageBicubic, cv::Mat& imageSR) {
    int pos1 = origImageFile.find_last_of("/");
    int pos2 = origImageFile.find_last_of(".");
    string imageName = origImageFile.substr(pos1 + 1, pos2 - pos1 - 1);
    string bicubicName = "./output/" + imageName + "_bicubic.png";
    string SRName = "./output/" + imageName  + "_" + DYNAMIC_LIST[modelType_] + "_" + ".png";
    //cv::imwrite(bicubicName, imageBicubic);
    cv::imwrite(SRName, imageSR);

}

void* SuperResolutionProcess::get_inference_output_item(uint32_t& itemDataSize,
                                              aclmdlDataset* inferenceOutput) {
    aclDataBuffer* dataBuffer = aclmdlGetDatasetBuffer(inferenceOutput, 0);
    if (dataBuffer == nullptr) {
        ERROR_LOG("Get the dataset buffer from model "

            "inference output failed");
        return nullptr;
    }

    void* dataBufferDev = aclGetDataBufferAddr(dataBuffer);
    if (dataBufferDev == nullptr) {
        ERROR_LOG("Get the dataset buffer address "
            "from model inference output failed");
        return nullptr;
    }

    size_t bufferSize = aclGetDataBufferSize(dataBuffer);
    // size_t bufferSize = 288*288*9*4;
    void* data = nullptr;
    if (runMode_ == ACL_HOST) {
        data = Utils::copy_data_device_to_local(dataBufferDev, bufferSize);
        if (data == nullptr) {
            ERROR_LOG("Copy inference output to host failed");
            return nullptr;
        }
    } else {
        data = dataBufferDev;
    }
    itemDataSize = bufferSize;
    return data;
}

void SuperResolutionProcess::destroy_resource()
{
    aclrtFree(inputBuf_);
    inputBuf_ = nullptr;
    aclError ret;
    if (stream_ != nullptr) {
        ret = aclrtDestroyStream(stream_);
        if (ret != ACL_ERROR_NONE) {
            ERROR_LOG("destroy stream failed");
        }
        stream_ = nullptr;
    }

    INFO_LOG("end to destroy stream");
    if (context_ != nullptr) {
        ret = aclrtDestroyContext(context_);
        if (ret != ACL_ERROR_NONE) {
            ERROR_LOG("destroy context failed");
        }
        context_ = nullptr;
    }
    INFO_LOG("end to destroy context");

    ret = aclrtResetDevice(deviceId_);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("reset device failed");
    }
    INFO_LOG("end to reset device is %d", deviceId_);
    ret = aclFinalize();
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("finalize acl failed");
    }
    INFO_LOG("end to finalize acl");
}




