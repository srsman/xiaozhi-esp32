/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>

void wear_levelling_init(void);
const char* wear_levelling_get_base_path(void);

#ifdef __cplusplus
}
#endif
