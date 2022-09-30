#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(naas_nordic)

# NORDIC SDK APP START
target_sources(app PRIVATE Naas_Rel_6.0/jiot_nidd_main.c)
target_sources_ifdef(CONFIG_NAAS_NORDIC app PRIVATE Nordic/jiot_nidd_plat_abs.c)
target_sources_ifdef(CONFIG_NAAS_NORDIC app PRIVATE Nordic/jiot_nidd_utils.c)
target_sources_ifdef(CONFIG_NAAS_NORDIC app PRIVATE Nordic/main.c)
# NORDIC SDK APP END


zephyr_include_directories(Naas_Rel_6.0)
