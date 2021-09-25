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

* File utils.cpp
* Description: handle file operations
*/
#include "utils.h"
#include <map>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <cstring>
#include <dirent.h>
#include <vector>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "acl/acl.h"

using namespace std;

namespace {
const std::string kImagePathSeparator = ",";
const int kStatSuccess = 0;
const std::string kFileSperator = "/";
const std::string kPathSeparator = "/";
// output image prefix
const std::string kOutputFilePrefix = "out_";
}

bool Utils::is_directory(const string &path) {
    // get path stat
    struct stat buf;
    if (stat(path.c_str(), &buf) != kStatSuccess) {
        return false;
    }

    // check
    if (S_ISDIR(buf.st_mode)) {
        return true;
    } else {
    return false;
    }
}

bool Utils::is_path_exist(const string &path) {
    ifstream file(path);
    if (!file) {
        return false;
    }
    return true;
}

void Utils::split_path(const string &path, vector<string> &path_vec) {
    char *char_path = const_cast<char*>(path.c_str());
    const char *char_split = kImagePathSeparator.c_str();
    char *tmp_path = strtok(char_path, char_split);
    while (tmp_path) {
        path_vec.emplace_back(tmp_path);
        tmp_path = strtok(nullptr, char_split);
    }
}

void Utils::get_all_files(const string &path, vector<string> &file_vec) {
    // split file path
    vector<string> path_vector;
    split_path(path, path_vector);

    for (string every_path : path_vector) {
        // check path exist or not
        if (!is_path_exist(path)) {
        ERROR_LOG("Failed to deal path=%s. Reason: not exist or can not access.",
                every_path.c_str());
        continue;
        }
        // get files in path and sub-path
        get_path_files(every_path, file_vec);
    }
}

void Utils::get_path_files(const string &path, vector<string> &file_vec) {
    struct dirent *dirent_ptr = nullptr;
    DIR *dir = nullptr;
    if (is_directory(path)) {
        dir = opendir(path.c_str());
        while ((dirent_ptr = readdir(dir)) != nullptr) {
            // skip . and ..
            if (dirent_ptr->d_name[0] == '.') {
            continue;
            }

            // file path
            string full_path = path + kPathSeparator + dirent_ptr->d_name;
            // directory need recursion
            if (is_directory(full_path)) {
                get_path_files(full_path, file_vec);
            } else {
                // put file
                file_vec.emplace_back(full_path);
            }
        }
    } 
    else {
        file_vec.emplace_back(path);
    }
}

void* Utils::copy_data_device_to_local(void* deviceData, uint32_t dataSize) {
    void* hostPtr = new uint8_t[dataSize];
    if (hostPtr == nullptr) {
        ERROR_LOG("malloc host data buffer failed");
        return nullptr;
    }

    aclError aclRet = aclrtMemcpy(hostPtr, dataSize, deviceData,
                                  dataSize, ACL_MEMCPY_DEVICE_TO_HOST);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("aclrtMemcpy device to host failed, aclRet is %d", aclRet);
        delete[]((uint8_t *)hostPtr);
        return nullptr;
    }

    return (uint8_t *)hostPtr;
}

/*

extern bool g_isDevice;

void* Utils::GetDeviceBufferOfFile(std::string &fileName, uint32_t &fileSize)
{
    uint32_t inputBuffSize = 0;
    void *inputBuff = Utils::ReadBinFile(fileName, inputBuffSize);
    if (inputBuff == nullptr) {
        return nullptr;
    }
    if (!g_isDevice) {
        void *inBufferDev = nullptr;
        aclError ret = aclrtMalloc(&inBufferDev, inputBuffSize, ACL_MEM_MALLOC_NORMAL_ONLY);
        if (ret != ACL_ERROR_NONE) {
            ERROR_LOG("malloc device buffer failed. size is %u, errorCode = %d.",
                inputBuffSize, static_cast<int32_t>(ret));
            aclrtFreeHost(inputBuff);
            inputBuff = nullptr;
            return nullptr;
        }

        ret = aclrtMemcpy(inBufferDev, inputBuffSize, inputBuff, inputBuffSize, ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != ACL_ERROR_NONE) {
            ERROR_LOG("memcpy failed. device buffer size is %u, input host buffer size is %u, errorCode = %d.",
                inputBuffSize, inputBuffSize, static_cast<int32_t>(ret));
            (void)aclrtFree(inBufferDev);
            inBufferDev = nullptr;
            (void)aclrtFreeHost(inputBuff);
            inputBuff = nullptr;
            return nullptr;
        }
        (void)aclrtFreeHost(inputBuff);
        inputBuff = nullptr;
        fileSize = inputBuffSize;
        return inBufferDev;
    } else {
        fileSize = inputBuffSize;
        return inputBuff;
    }
}

void* Utils::ReadBinFile(std::string &fileName, uint32_t &fileSize)
{
    if (CheckPathIsFile(fileName) == FAILED) {
        ERROR_LOG("%s is not a file", fileName.c_str());
        return nullptr;
    }

    std::ifstream binFile(fileName, std::ifstream::binary);
    if (binFile.is_open() == false) {
        ERROR_LOG("open file %s failed.", fileName.c_str());
        return nullptr;
    }

    binFile.seekg(0, binFile.end);
    uint32_t binFileBufferLen = binFile.tellg();
    if (binFileBufferLen == 0) {
        ERROR_LOG("binfile is empty, filename is %s.", fileName.c_str());
        binFile.close();
        return nullptr;
    }
    binFile.seekg(0, binFile.beg);

    void *binFileBufferData = nullptr;
    aclError ret = ACL_ERROR_NONE;
    if (!g_isDevice) {
        ret = aclrtMallocHost(&binFileBufferData, binFileBufferLen);
        if (binFileBufferData == nullptr) {
            ERROR_LOG("malloc binFileBufferData failed, errorCode = %d.", static_cast<int32_t>(ret));
            binFile.close();
            return nullptr;
        }
    } else {
        ret = aclrtMalloc(&binFileBufferData, binFileBufferLen, ACL_MEM_MALLOC_NORMAL_ONLY);
        if (ret != ACL_ERROR_NONE) {
            ERROR_LOG("malloc device buffer failed. size is %u, errorCode = %d.",
                binFileBufferLen, static_cast<int32_t>(ret));
            binFile.close();
            return nullptr;
        }
    }
    binFile.read(static_cast<char *>(binFileBufferData), binFileBufferLen);
    binFile.close();
    fileSize = binFileBufferLen;
    return binFileBufferData;
}

Result Utils::CheckPathIsFile(const std::string &fileName)
{
#if defined(_MSC_VER)
    DWORD bRet = GetFileAttributes((LPCSTR)fileName.c_str());
    if (bRet == FILE_ATTRIBUTE_DIRECTORY) {
        ERROR_LOG("%s is not a file, please enter a file", fileName.c_str());
        return FAILED;
    }
#else
    struct stat sBuf;
    int fileStatus = stat(fileName.data(), &sBuf);
    if (fileStatus == -1) {
        ERROR_LOG("failed to get file");
        return FAILED;
    }
    if (S_ISREG(sBuf.st_mode) == 0) {
        ERROR_LOG("%s is not a file, please enter a file", fileName.c_str());
        return FAILED;
    }
#endif

    return SUCCESS;
}
*/