/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD & Antigravity
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "utils/config_ap/config_ap.h"

#include <assets/assets.h>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <mooncake_log.h>

static const std::string_view _tag = "HAL-Badge";

namespace {

constexpr const char* _badge_dir            = "/spiflash/badge";
constexpr const char* _active_slot_file     = "/spiflash/badge/active_slot.txt";
constexpr std::size_t _max_badge_slot_count = 6;
constexpr std::size_t _builtin_badge_count  = 3;

std::string slot_meta_path(std::size_t slot)
{
    char path[96] = {};
    snprintf(path, sizeof(path), "%s/slot_%u.meta", _badge_dir, static_cast<unsigned>(slot));
    return std::string(path);
}

std::string slot_image_path(std::size_t slot, std::string_view extension)
{
    char path[96] = {};
    snprintf(path, sizeof(path), "%s/slot_%u.%.*s", _badge_dir, static_cast<unsigned>(slot),
             static_cast<int>(extension.size()), extension.data());
    return std::string(path);
}

bool ensure_badge_dir()
{
    if (mkdir(_badge_dir, 0775) == 0 || errno == EEXIST) {
        return true;
    }
    mclog::tagError(_tag, "failed to create badge dir: {}, errno={}", _badge_dir, errno);
    return false;
}

bool write_text_file(const std::string& path, std::string_view text)
{
    FILE* file = fopen(path.c_str(), "wb");
    if (file == nullptr) {
        mclog::tagError(_tag, "failed to open file for write: {}", path);
        return false;
    }
    const size_t written = fwrite(text.data(), 1, text.size(), file);
    fclose(file);
    return written == text.size();
}

bool write_binary_file(const std::string& path, const std::vector<uint8_t>& data)
{
    FILE* file = fopen(path.c_str(), "wb");
    if (file == nullptr) {
        mclog::tagError(_tag, "failed to open file for write: {}", path);
        return false;
    }
    const size_t written = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), file);
    fclose(file);
    return written == data.size();
}

std::string read_text_file(const std::string& path)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return {};
    }
    char buffer[32] = {};
    const size_t size = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);

    std::string text(buffer, size);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
        text.pop_back();
    }
    return text;
}

std::string normalize_extension(std::string file_name, std::string content_type)
{
    auto normalize = [](std::string& text) {
        for (char& ch : text) {
            if (ch >= 'A' && ch <= 'Z') {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }
    };

    normalize(file_name);
    normalize(content_type);

    const auto dot = file_name.find_last_of('.');
    if (dot != std::string::npos) {
        const std::string ext = file_name.substr(dot + 1);
        if (ext == "png" || ext == "jpg" || ext == "jpeg") {
            return ext == "jpeg" ? "jpg" : ext;
        }
    }

    if (content_type.find("png") != std::string::npos) {
        return "png";
    }
    if (content_type.find("jpeg") != std::string::npos || content_type.find("jpg") != std::string::npos) {
        return "jpg";
    }

    return {};
}

std::size_t read_active_slot()
{
    const std::string text = read_text_file(_active_slot_file);
    if (text.empty()) {
        return 0;
    }
    const unsigned long slot = strtoul(text.c_str(), nullptr, 10);
    if (slot >= _max_badge_slot_count) {
        return 0;
    }
    return static_cast<std::size_t>(slot);
}

std::string read_slot_extension(std::size_t slot)
{
    std::string ext = read_text_file(slot_meta_path(slot));
    if (ext == "png" || ext == "jpg") {
        return ext;
    }

    for (const auto* candidate : {"png", "jpg", "jpeg"}) {
        const std::string path = slot_image_path(slot, candidate);
        if (access(path.c_str(), R_OK) == 0) {
            return std::string(strcmp(candidate, "jpeg") == 0 ? "jpg" : candidate);
        }
    }
    return {};
}

std::string slot_content_type(std::string_view extension)
{
    if (extension == "jpg" || extension == "jpeg") {
        return "image/jpeg";
    }
    if (extension == "png") {
        return "image/png";
    }
    return "application/octet-stream";
}

bool read_binary_file(const std::string& path, std::vector<uint8_t>& data)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    const long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return false;
    }
    rewind(file);
    data.resize(static_cast<std::size_t>(size));
    const size_t read_size = data.empty() ? 0 : fread(data.data(), 1, data.size(), file);
    fclose(file);
    return read_size == data.size();
}

bool is_slot_available(std::size_t slot)
{
    return !read_slot_extension(slot).empty();
}

bool persist_active_slot(std::size_t slot)
{
    if (slot >= _max_badge_slot_count) {
        return false;
    }
    return write_text_file(_active_slot_file, std::to_string(slot));
}

bool clear_active_slot()
{
    if (unlink(_active_slot_file) == 0 || errno == ENOENT) {
        return true;
    }
    return false;
}

std::vector<std::size_t> get_available_user_slots()
{
    std::vector<std::size_t> slots;
    for (std::size_t s = 0; s < _max_badge_slot_count; ++s) {
        if (is_slot_available(s)) {
            slots.push_back(s);
        }
    }
    return slots;
}

bool load_badge_slot(lv_obj_t* image, std::size_t slot)
{
    if (image == nullptr || slot >= _max_badge_slot_count) {
        return false;
    }
    const std::string extension = read_slot_extension(slot);
    if (extension.empty()) {
        return false;
    }
    const std::string fs_path = slot_image_path(slot, extension);
    if (access(fs_path.c_str(), R_OK) != 0) {
        return false;
    }
    const std::string lvgl_path = std::string("A:") + fs_path;
    lv_image_set_src(image, lvgl_path.c_str());
    persist_active_slot(slot);
    mclog::tagInfo(_tag, "load badge image from {}, slot={}", fs_path, slot);
    return true;
}

/* -------------------------------------------------------------------------- */
/*                       Built-in HD Geek Badges (466x466)                   */
/* -------------------------------------------------------------------------- */
static constexpr int _badge_w = 466;
static constexpr int _badge_h = 466;
static lv_image_dsc_t s_builtin_dsc[3] = {};
static uint16_t* s_builtin_bufs[3]     = {nullptr, nullptr, nullptr};
static size_t s_current_carousel_idx   = 0;

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static void render_builtin_badge(size_t index)
{
    if (index >= _builtin_badge_count || s_builtin_bufs[index] != nullptr) {
        return;
    }

    s_builtin_bufs[index] = (uint16_t*)heap_caps_malloc(_badge_w * _badge_h * sizeof(uint16_t),
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_builtin_bufs[index]) {
        return;
    }

    uint16_t* buf = s_builtin_bufs[index];
    const int cx = _badge_w / 2;
    const int cy = _badge_h / 2;

    for (int y = 0; y < _badge_h; ++y) {
        for (int x = 0; x < _badge_w; ++x) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy);

            uint8_t r = 0, g = 0, b = 0;

            if (index == 0) {
                // Badge 0: 小智 AI · 极客赛博工牌 (XiaoZhi Cyber Agent Badge)
                float t = dist / 233.0f;
                if (t > 1.0f) t = 1.0f;
                // Deep dark cyber blue to midnight
                r = (uint8_t)(8 * (1.0f - t * 0.7f));
                g = (uint8_t)(24 * (1.0f - t * 0.8f));
                b = (uint8_t)(48 * (1.0f - t * 0.8f));

                // Glowing outer radar HUD ring
                if (dist >= 210 && dist <= 214) {
                    r = 0; g = 240; b = 255;
                } else if (dist >= 190 && dist <= 191) {
                    r = 0; g = 180; b = 200;
                }
                // Ticks around HUD
                if (dist >= 200 && dist <= 208) {
                    float angle = atan2f(dy, dx) * 180.0f / 3.14159f;
                    int deg = (int)(angle + 360.0f) % 360;
                    if (deg % 15 == 0) {
                        r = 0; g = 255; b = 200;
                    }
                }
                // Central badge card outline
                if (dist >= 120 && dist <= 122) {
                    r = 0; g = 255; b = 180;
                }
            } else if (index == 1) {
                // Badge 1: 深空领航员 · 星际通行证 (Cosmic Navigator Pass)
                float t = dist / 233.0f;
                if (t > 1.0f) t = 1.0f;
                // Nebula violet gradient
                r = (uint8_t)(28 * (1.0f - t * 0.7f));
                g = (uint8_t)(10 * (1.0f - t * 0.7f));
                b = (uint8_t)(52 * (1.0f - t * 0.7f));

                // Orbit rings
                if (dist >= 212 && dist <= 214) {
                    r = 255; g = 215; b = 0; // Golden border
                }
                if (dist >= 150 && dist <= 151) {
                    r = 180; g = 120; b = 255;
                }
                if (dist >= 90 && dist <= 91) {
                    r = 220; g = 180; b = 255;
                }
            } else {
                // Badge 2: 极客骇客 · 数字矩阵 (Cyber Matrix Developer)
                // Matrix dark green
                r = 0;
                g = (uint8_t)(18 * (1.0f - (dist / 233.0f) * 0.5f));
                b = 8;

                // Matrix grid lines
                if ((x % 32 == 0 || y % 32 == 0) && dist < 215) {
                    g += 18;
                }
                // Terminal green border
                if (dist >= 210 && dist <= 213) {
                    r = 0; g = 255; b = 100;
                }
                if (dist >= 130 && dist <= 132) {
                    r = 0; g = 200; b = 80;
                }
            }

            // Blit centered icons into badge
            const lv_image_dsc_t* icon = (index == 0) ? &icon_xiaozhi : ((index == 1) ? &icon_badge : &icon_edit_badge);
            if (icon && icon->data) {
                int iw = icon->header.w;
                int ih = icon->header.h;
                int ix = x - (cx - iw / 2);
                int iy = y - (cy - ih / 2);
                if (ix >= 0 && ix < iw && iy >= 0 && iy < ih) {
                    const uint8_t* p = icon->data + (iy * iw + ix) * 3;
                    uint8_t ir = p[2], ig = p[1], ib = p[0];
                    if (ir > 20 || ig > 20 || ib > 20) {
                        r = ir; g = ig; b = ib;
                    }
                }
            }

            buf[y * _badge_w + x] = rgb565(r, g, b);
        }
    }

    s_builtin_dsc[index].header.magic = LV_IMAGE_HEADER_MAGIC;
    s_builtin_dsc[index].header.cf    = LV_COLOR_FORMAT_RGB565;
    s_builtin_dsc[index].header.w     = _badge_w;
    s_builtin_dsc[index].header.h     = _badge_h;
    s_builtin_dsc[index].header.stride = _badge_w * 2;
    s_builtin_dsc[index].data_size    = _badge_w * _badge_h * 2;
    s_builtin_dsc[index].data         = (const uint8_t*)buf;
}

void load_builtin_badge(lv_obj_t* image, size_t index)
{
    if (index >= _builtin_badge_count) {
        index = 0;
    }
    render_builtin_badge(index);
    if (s_builtin_bufs[index]) {
        lv_image_set_src(image, &s_builtin_dsc[index]);
    } else {
        lv_image_set_src(image, &icon_xiaozhi);
    }
}

/* -------------------------------------------------------------------------- */
/*                         Config AP Callback Handlers                        */
/* -------------------------------------------------------------------------- */
badge::config_ap::BadgeState make_badge_state()
{
    badge::config_ap::BadgeState state;
    state.slotCount  = _max_badge_slot_count;
    state.activeSlot = read_active_slot();
    state.slots.reserve(_max_badge_slot_count);

    for (std::size_t slot = 0; slot < _max_badge_slot_count; ++slot) {
        state.slots.push_back({
            .slot     = slot,
            .hasImage = is_slot_available(slot),
            .isActive = slot == state.activeSlot,
        });
    }
    return state;
}

bool get_badge_image(std::size_t slot, badge::config_ap::ImageData& image, std::string& message)
{
    if (slot >= _max_badge_slot_count) {
        message = "invalid badge slot";
        return false;
    }
    const std::string extension = read_slot_extension(slot);
    if (extension.empty()) {
        message = "badge image not found";
        return false;
    }
    const std::string path = slot_image_path(slot, extension);
    if (!read_binary_file(path, image.data)) {
        message = "failed to read image file";
        return false;
    }
    image.contentType = slot_content_type(extension);
    return true;
}

bool set_active_badge_slot(std::size_t slot, std::string& message)
{
    if (slot >= _max_badge_slot_count) {
        message = "invalid badge slot";
        return false;
    }
    if (!is_slot_available(slot)) {
        message = "badge image not found";
        return false;
    }
    if (!persist_active_slot(slot)) {
        message = "failed to update active slot";
        return false;
    }
    message = "active slot updated";
    return true;
}

bool delete_badge_slot(std::size_t slot, std::string& message)
{
    if (slot >= _max_badge_slot_count) {
        message = "invalid badge slot";
        return false;
    }
    bool deleted = false;
    for (const auto* candidate : {"png", "jpg", "jpeg"}) {
        const std::string path = slot_image_path(slot, candidate);
        if (unlink(path.c_str()) == 0) {
            deleted = true;
        }
    }
    const std::string meta_path = slot_meta_path(slot);
    if (unlink(meta_path.c_str()) == 0) {
        deleted = true;
    }
    if (!deleted) {
        message = "badge image not found";
        return false;
    }
    if (read_active_slot() == slot) {
        auto user_slots = get_available_user_slots();
        if (!user_slots.empty()) {
            persist_active_slot(user_slots[0]);
        } else {
            clear_active_slot();
        }
    }
    message = "badge image deleted";
    return true;
}

bool save_badge_upload(const badge::config_ap::UploadRequest& request, std::string& message)
{
    if (request.slot >= _max_badge_slot_count) {
        message = "invalid badge slot";
        return false;
    }
    if (request.data.empty()) {
        message = "empty upload body";
        return false;
    }
    if (!ensure_badge_dir()) {
        message = "failed to create badge directory";
        return false;
    }

    const std::string extension = normalize_extension(request.fileName, request.contentType);
    if (extension != "jpg" && extension != "png") {
        message = "only jpg/png images are supported";
        return false;
    }

    const std::string final_path = slot_image_path(request.slot, extension);
    const std::string temp_path  = final_path + ".tmp";
    if (!write_binary_file(temp_path, request.data)) {
        message = "failed to store image";
        return false;
    }

    if (unlink(final_path.c_str()) != 0 && errno != ENOENT) {
        unlink(temp_path.c_str());
        message = "failed to replace existing image";
        return false;
    }

    if (rename(temp_path.c_str(), final_path.c_str()) != 0) {
        unlink(temp_path.c_str());
        message = "failed to finalize image";
        return false;
    }

    for (const auto* candidate : {"png", "jpg", "jpeg"}) {
        if (extension == candidate || (extension == "jpg" && strcmp(candidate, "jpeg") == 0)) {
            continue;
        }
        unlink(slot_image_path(request.slot, candidate).c_str());
    }

    if (!write_text_file(slot_meta_path(request.slot), extension)) {
        message = "failed to store image metadata";
        return false;
    }
    if (!write_text_file(_active_slot_file, std::to_string(request.slot))) {
        message = "failed to store active slot";
        return false;
    }

    mclog::tagInfo(_tag, "badge image saved, slot={}, path={}", request.slot, final_path);
    message = "upload success";
    return true;
}

} // namespace

/* -------------------------------------------------------------------------- */
/*                               HAL Implementations                          */
/* -------------------------------------------------------------------------- */
bool Hal::loadBadgeImage(lv_obj_t* image)
{
    if (image == nullptr) {
        return false;
    }

    auto user_slots = get_available_user_slots();
    if (!user_slots.empty()) {
        size_t active = read_active_slot();
        if (is_slot_available(active)) {
            s_current_carousel_idx = 0;
            for (size_t i = 0; i < user_slots.size(); ++i) {
                if (user_slots[i] == active) {
                    s_current_carousel_idx = i;
                    break;
                }
            }
            return load_badge_slot(image, active);
        } else {
            s_current_carousel_idx = 0;
            return load_badge_slot(image, user_slots[0]);
        }
    }

    // No user uploaded images yet: display gorgeous Built-in Badge 0
    s_current_carousel_idx = 0;
    load_builtin_badge(image, 0);
    return true;
}

bool Hal::loadNextBadgeImage(lv_obj_t* image)
{
    if (image == nullptr) {
        return false;
    }
    auto user_slots = get_available_user_slots();
    size_t total_count = user_slots.size() + _builtin_badge_count;
    s_current_carousel_idx = (s_current_carousel_idx + 1) % total_count;

    if (s_current_carousel_idx < user_slots.size()) {
        return load_badge_slot(image, user_slots[s_current_carousel_idx]);
    } else {
        size_t builtin_idx = s_current_carousel_idx - user_slots.size();
        load_builtin_badge(image, builtin_idx);
        return true;
    }
}

bool Hal::loadPreviousBadgeImage(lv_obj_t* image)
{
    if (image == nullptr) {
        return false;
    }
    auto user_slots = get_available_user_slots();
    size_t total_count = user_slots.size() + _builtin_badge_count;
    s_current_carousel_idx = (s_current_carousel_idx + total_count - 1) % total_count;

    if (s_current_carousel_idx < user_slots.size()) {
        return load_badge_slot(image, user_slots[s_current_carousel_idx]);
    } else {
        size_t builtin_idx = s_current_carousel_idx - user_slots.size();
        load_builtin_badge(image, builtin_idx);
        return true;
    }
}

void Hal::startBadgeEditModeViaAp(std::function<void(std::string_view)> onLog)
{
    mclog::tagInfo(_tag, "start badge edit mode via AP");

    if (!ensure_badge_dir()) {
        if (onLog) {
            onLog("Failed to create badge storage directory");
        }
        return;
    }

    badge::config_ap::run(onLog, {
        .onUpload    = save_badge_upload,
        .onGetState  = make_badge_state,
        .onGetImage  = get_badge_image,
        .onSetActive = set_active_badge_slot,
        .onDelete    = delete_badge_slot,
    });

    // Re-connect WiFi station after AP edit mode exits
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_connect();
}
