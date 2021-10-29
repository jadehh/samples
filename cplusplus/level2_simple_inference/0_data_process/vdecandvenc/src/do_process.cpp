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
#include <iostream>

#include "acl/acl.h"
#include "atlas_model.h"
#include "do_process.h"
using namespace std;

DoProcess::DoProcess()
:deviceId_(0), context_(nullptr), stream_(nullptr),  isInited_(false){}

DoProcess::~DoProcess() {
    DestroyResource();
}

AtlasError DoProcess::InitResource() {
    /* 1. ACL初始化 */
    char aclConfigPath[32] = {'\0'};
    aclInit(aclConfigPath);

    /* 2. 运行管理资源申请,包括Device、Context、Stream */
    aclrtSetDevice(0);
    // create stream
    aclError ret = aclrtCreateStream(&stream_);
    if (ret != ACL_ERROR_NONE) {
        ATLAS_LOG_ERROR("acl create stream failed\n");
        return ATLAS_ERROR_CREATE_STREAM;
    }
    ATLAS_LOG_INFO("create stream success");

    ret = aclrtGetRunMode(&runMode_);
    if (ret != ACL_ERROR_NONE) {
        ATLAS_LOG_ERROR("acl get run mode failed\n");
        return ATLAS_ERROR_GET_RUM_MODE;
    }

    return ATLAS_OK;
}

AtlasError DoProcess::Init() {
    if (isInited_) {
        ATLAS_LOG_INFO("instance is initied already!\n");
        return ATLAS_OK;
    }
    AtlasError ret = InitResource();
    if (ret != ATLAS_OK) {
        ATLAS_LOG_ERROR("Init acl resource failed, error: %d", ret);
        return ret;
    }

    ret = dvpp_.InitResource(stream_);
    if (ret != ATLAS_OK) {
        ATLAS_LOG_ERROR("Init dvpp failed\n");
        return ret;
    }

    isInited_ = true;
    return ATLAS_OK;
}

AtlasError DoProcess::Process(ImageData& image) {
    AtlasError ret = encoder_.DoVencProcess(image);
    if(ret != ATLAS_OK) {
        ATLAS_LOG_ERROR("video encode failed\n");
        return ATLAS_ERROR;
    }
    return ATLAS_OK;
}
AtlasError DoProcess::Set(uint32_t width, uint32_t height) {
    AtlasError ret = encoder_.InitResource(width, height);
    if (ret != ATLAS_OK) {
        ATLAS_LOG_ERROR("Init video_encoder failed\n");
        return ret;
    }
    return ATLAS_OK;
}

void DoProcess::DestroyResource()
{
    dvpp_.DestroyResource();
    encoder_.DestroyResource();

    aclError ret;
    if (stream_ != nullptr) {
        ret = aclrtDestroyStream(stream_);
        if (ret != ACL_ERROR_NONE) {
            ATLAS_LOG_ERROR("destroy stream failed");
        }
        stream_ = nullptr;
    }

    ret = aclrtResetDevice(deviceId_);
    if (ret != ACL_ERROR_NONE) {
        ATLAS_LOG_ERROR("reset device failed\n");
    }
    ATLAS_LOG_INFO("end to reset device is %d\n", deviceId_);

    ret = aclFinalize();
    if (ret != ACL_ERROR_NONE) {
        ATLAS_LOG_ERROR("finalize acl failed\n");
    }
    ATLAS_LOG_INFO("end to finalize acl");

}
