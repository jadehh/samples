#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @File     : setup.py.py
# @Author   : jade
# @Date     : 2022/12/1 14:06
# @Email    : jadehh@1ive.com
# @Software : Samples
# @Desc     :
from setuptools import setup
import os
with open("README.md","r") as fh:
    long_description = fh.read()

def find_packages(path,pack_list):
    for file_path in os.listdir(path):
        if os.path.isdir(os.path.join(path,file_path)):
            if "__pycache__" != file_path:
                new_path = path.replace("/", ".")
                if new_path:
                    pack_list.append("{}.{}".format(new_path,file_path))
                else:
                    pack_list.append("{}.{}".format(path, file_path))
                find_packages(os.path.join(path,file_path),pack_list)


if __name__ == '__main__':
    pack_list = ["acllite"]
    find_packages("acllite",pack_list)
setup(
    name="acllite",
    version="1.0.2",
    keywords=("pip", "acllite", ""),
    description="acllite",
    long_description=long_description,
    long_description_content_type="text/markdown",
    license="MIT Licence",
    url="https://jadehh@live.com",
    author="jade",
    author_email="jadehh@live.com",
    packages=pack_list,
    package_data={'': ['*.so', "*.cpp", "lib*","*.h","*.proto","*.md"]},
    include_package_data=True,
    platforms="any",
    install_requires=[]  # 这个项目需要的第三方库
)
