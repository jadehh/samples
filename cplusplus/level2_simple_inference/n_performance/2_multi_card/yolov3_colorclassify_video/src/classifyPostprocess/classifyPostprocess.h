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

* File sample_process.h
* Description: handle acl resource
*/
#pragma once
#include <iostream>
#include <mutex>
#include <unistd.h>
#include "acl/acl.h"
#include "VideoCapture.h"
#include "AclLiteThread.h"
#include "AclLiteImageProc.h"
#include "params.h"

class ClassifyPostprocessThread : public AclLiteThread {
public:
    ClassifyPostprocessThread(const char*& configFile, int channelId);
    ~ClassifyPostprocessThread();

    AclLiteError Init();
    AclLiteError Process(int msgId, shared_ptr<void> msgData);

private:
    AclLiteError DrawResult(shared_ptr<CarDetectDataMsg> &carDetectDataMsg);
    AclLiteError InferOutputProcess(shared_ptr<CarDetectDataMsg> carDetectDataMsg);
    AclLiteError GetBaseConfig(int& videoWidth, int& videoHeight, uint32_t channelId);

private:
    const char* configFile_;
    int videoWidth_;
    int videoHeight_;
    int channelId_;
    cv::VideoWriter outputVideo_;
};

