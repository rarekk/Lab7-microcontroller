cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(module4)

# NOTE: The custom partition table is enabled via sdkconfig.defaults:
#   CONFIG_PARTITION_TABLE_CUSTOM=y
#   CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
