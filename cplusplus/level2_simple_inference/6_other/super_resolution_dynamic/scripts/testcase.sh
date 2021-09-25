


data_source="https://c7xcode.obs.cn-north-4.myhuaweicloud.com/models/super_resolution/test_image/"
data_name="butterfly_GT.bmp"
verify_source="https://c7xcode.obs.cn-north-4.myhuaweicloud.com/models/super_resolution/verify_image/"
verify_name="butterfly_GT_SRCNN.png"

model_path="https://modelzoo-train-atc.obs.cn-north-4.myhuaweicloud.com/003_Atc_Models/AE/ATC%20Model/super_resolution/FSRCNN/FSRCNN_x3.om"
caffe_model="https://modelzoo-train-atc.obs.cn-north-4.myhuaweicloud.com/003_Atc_Models/AE/ATC%20Model/super_resolution/FSRCNN/FSRCNN.caffemodel"
caffe_prototxt="https://modelzoo-train-atc.obs.cn-north-4.myhuaweicloud.com/003_Atc_Models/AE/ATC%20Model/super_resolution/FSRCNN/FSRCNN.prototxt"
model_name="FSRCNN_x3"

data_source="https://c7xcode.obs.cn-north-4.myhuaweicloud.com/models/super_resolution/test_image/"
verify_source="https://c7xcode.obs.cn-north-4.myhuaweicloud.com/models/super_resolution/verify_image/"
project_name="cplusplus_Super_Resolution"

version=$1

script_path="$( cd "$(dirname $BASH_SOURCE)" ; pwd -P)"
project_path=${script_path}/..

common_script_dir=${script_path}/../../../../../common/
run_command="./main ../data 256 256"
#model_atc="atc --model=${project_path}/model/${caffe_prototxt##*/} --weight=${project_path}/model/${caffe_model##*/} --framework=0 --output=${HOME}/models/${project_name}/${model_name} --soc_version=Ascend310 --input_format=NCHW --input_shape="data: 1, 1, -1, -1" --dynamic_image_size="256,256;512,512;288,288" --output_type=FP32
#"

declare -i success=0
declare -i inferenceError=1
declare -i verifyResError=2

. ${common_script_dir}/testcase_common.sh

function main() {

    # 下载测试集和验证集
    downloadDataWithVerifySource
    if [ $? -ne 0 ];then
        return ${inferenceError}
    fi

    # 转模型
    #modelconvert
    cd ${project_path}/model/
    wget -N https://modelzoo-train-atc.obs.cn-north-4.myhuaweicloud.com/003_Atc_Models/AE/ATC%20Model/super_resolution/FSRCNN/FSRCNN_x3.om --no-check-certificate


    buildproject
    if [ $? -ne 0 ];then
        return ${inferenceError}
    fi

    run_picture
    if [ $? -eq ${inferenceError} ];then
        return ${inferenceError}
    elif [ $? -eq ${verifyResError} ];then
	return ${verifyResError}
    fi

    return ${success}
}
main

