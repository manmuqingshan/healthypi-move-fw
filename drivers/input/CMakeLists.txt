# SPDX-License-Identifier: Apache-2.0

zephyr_library_amend()
zephyr_library_property(ALLOW_EMPTY TRUE)

zephyr_library_sources_ifdef(CONFIG_INPUT_CHSC5816 input_chsc5816.c)
