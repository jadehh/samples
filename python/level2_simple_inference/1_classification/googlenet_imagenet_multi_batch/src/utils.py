import acl
from constants import *

def check_ret(message, ret):
    if ret != ACL_ERROR_NONE:
        raise Exception("{} failed ret={}"
                        .format(message, ret))


def copy_data_device_to_host(device_data, data_size):
    host_buffer, ret = acl.rt.malloc_host(int(data_size))
    check_ret("acl.rt.malloc_host", ret)
    ret = acl.rt.memcpy(host_buffer, data_size,
                        device_data, data_size,
                        ACL_MEMCPY_DEVICE_TO_HOST)
    check_ret("acl.rt.memcpy", ret)
    return host_buffer

def copy_data_device_to_device(device_data, data_size):
    host_buffer, ret = acl.rt.malloc(data_size, ACL_MEM_MALLOC_HUGE_FIRST)
    check_ret("acl.rt.malloc_host", ret)
    ret = acl.rt.memcpy(host_buffer, data_size,
                        device_data, data_size,
                        ACL_MEMCPY_DEVICE_TO_DEVICE)
    check_ret("acl.rt.memcpy", ret)
    return host_buffer

def copy_data_host_to_device(device_data, data_size):
    host_buffer, ret = acl.rt.malloc(data_size, ACL_MEM_MALLOC_HUGE_FIRST)
    check_ret("acl.rt.malloc_host", ret)
    ret = acl.rt.memcpy(host_buffer, data_size,
                        device_data, data_size,
                        ACL_MEMCPY_HOST_TO_DEVICE)
    check_ret("acl.rt.memcpy", ret)
    return host_buffer

def copy_data_to_dvpp(data, size, run_mode):
    policy = ACL_MEMCPY_HOST_TO_DEVICE
    if run_mode == ACL_DEVICE:
        policy = ACL_MEMCPY_DEVICE_TO_DEVICE

    buffer, ret = acl.media.dvpp_malloc(size)
    check_ret("acl.rt.malloc_host", ret)
    ret = acl.rt.memcpy(buffer, size, data, size, policy)
    check_ret("acl.rt.memcpy", ret)

    return buffer


def align_up(value, align):
    """
    align input data
    """
    return int(int((value + align - 1) / align) * align)


def align_up128(value):
    """
    128 alignment of input data
    """
    return align_up(value, 128)


def align_up16(value):
    """
    16 alignment of input data
    """
    return align_up(value, 16)


def align_up2(value):
    """
    2 alignment of input data
    """
    return align_up(value, 2)


def yuv420sp_size(width, height):
    """
    yuv image size
    """
    return int(width * height * 3 /2)
