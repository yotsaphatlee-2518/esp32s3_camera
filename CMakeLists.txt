cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32s3_camera)

# เพิ่มบรรทัดนี้เพื่อระบุตำแหน่งของ component ภายนอก
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components/esp32-camera)
