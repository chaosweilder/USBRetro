// pad_config_storage_esp32.c - ESP32 NVS storage for pad config
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith

#include "pad/pad_config_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

#define NVS_NAMESPACE "joypad"
#define NVS_PAD_KEY   "padcfg"

static nvs_handle_t nvs_hdl;
static bool nvs_opened = false;
static uint32_t current_seq = 0;

void pad_config_storage_init(void) {
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_hdl);
    if (err != ESP_OK) {
        printf("[pad_config] NVS open failed: %s\n", esp_err_to_name(err));
        return;
    }
    nvs_opened = true;

    // Check if config exists
    pad_config_flash_t tmp;
    size_t size = sizeof(tmp);
    err = nvs_get_blob(nvs_hdl, NVS_PAD_KEY, &tmp, &size);
    if (err == ESP_OK && size == sizeof(tmp) && tmp.magic == PAD_CONFIG_MAGIC) {
        current_seq = tmp.sequence;
        printf("[pad_config] ESP32 NVS: found config (seq=%lu)\n", (unsigned long)current_seq);
    } else {
        printf("[pad_config] ESP32 NVS: no saved config\n");
    }
}

bool pad_config_storage_load(pad_config_flash_t* out) {
    if (!nvs_opened) return false;

    size_t size = sizeof(pad_config_flash_t);
    esp_err_t err = nvs_get_blob(nvs_hdl, NVS_PAD_KEY, out, &size);
    if (err != ESP_OK || size != sizeof(pad_config_flash_t)) return false;
    if (out->magic != PAD_CONFIG_MAGIC) return false;

    return true;
}

void pad_config_storage_save(const pad_config_flash_t* config) {
    if (!nvs_opened) {
        printf("[pad_config] NVS not opened\n");
        return;
    }

    pad_config_flash_t write_data;
    memcpy(&write_data, config, sizeof(write_data));
    write_data.magic = PAD_CONFIG_MAGIC;
    write_data.sequence = ++current_seq;

    esp_err_t err = nvs_set_blob(nvs_hdl, NVS_PAD_KEY, &write_data, sizeof(write_data));
    if (err != ESP_OK) {
        printf("[pad_config] NVS write failed: %s\n", esp_err_to_name(err));
        return;
    }

    err = nvs_commit(nvs_hdl);
    if (err != ESP_OK) {
        printf("[pad_config] NVS commit failed: %s\n", esp_err_to_name(err));
        return;
    }

    printf("[pad_config] Saved to NVS (seq=%lu)\n", (unsigned long)write_data.sequence);
}

void pad_config_storage_erase(void) {
    if (!nvs_opened) return;

    nvs_erase_key(nvs_hdl, NVS_PAD_KEY);
    nvs_commit(nvs_hdl);
    current_seq = 0;
    printf("[pad_config] NVS config erased\n");
}
