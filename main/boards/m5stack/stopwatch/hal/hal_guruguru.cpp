/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD & Antigravity
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <dns_server.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <lwip/ip_addr.h>
#include <nvs_flash.h>
#include <mooncake_log.h>

namespace {

constexpr const char* TAG                    = "HAL-Guruguru";
constexpr const char* _guru_dir              = "/spiflash/guruguru";
constexpr const char* _ap_url                = "http://192.168.4.1";
constexpr EventBits_t _exit_bit              = BIT0;
constexpr std::size_t _max_upload_size_bytes = 2 * 1024 * 1024;

extern const char guruguru_config_ap_html_start[] asm("_binary_guruguru_config_ap_html_start");
extern const char guruguru_config_ap_html_end[] asm("_binary_guruguru_config_ap_html_end");

constexpr const char* _captive_portal_urls[] = {
    "/hotspot-detect.html",       "/generate_204*", "/mobile/status.php",
    "/check_network_status.txt",  "/ncsi.txt",      "/fwlink/",
    "/connectivity-check.html",   "/success.txt",   "/portal.html",
    "/library/test/success.html",
};

EventGroupHandle_t s_guru_event_group = nullptr;

bool ensure_guru_dir()
{
    if (mkdir(_guru_dir, 0775) == 0 || errno == EEXIST) {
        return true;
    }
    ESP_LOGE(TAG, "failed to create guruguru dir: %s, errno=%d", _guru_dir, errno);
    return false;
}

bool file_exists(const std::string& path)
{
    struct stat st = {};
    return (stat(path.c_str(), &st) == 0);
}

bool write_binary_file(const std::string& path, const std::vector<uint8_t>& data)
{
    FILE* file = fopen(path.c_str(), "wb");
    if (file == nullptr) {
        ESP_LOGE(TAG, "failed to open %s for write: errno=%d", path.c_str(), errno);
        return false;
    }
    const std::size_t written = fwrite(data.data(), 1, data.size(), file);
    fclose(file);
    return written == data.size();
}

bool read_binary_file(const std::string& path, std::vector<uint8_t>& data)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    const long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return false;
    }

    data.resize(static_cast<std::size_t>(file_size));
    const std::size_t read_bytes = fread(data.data(), 1, data.size(), file);
    fclose(file);
    return read_bytes == data.size();
}

std::string make_ap_ssid()
{
    uint8_t mac[6] = {};
    esp_err_t ret  = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (ret != ESP_OK) {
        return "M5Guruguru-Avatar";
    }
    char suffix[8] = {};
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    return std::string("M5Guruguru-") + suffix;
}

bool init_wifi_stack()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable         = false;
    ret                    = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "wifi init failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

class GuruguruApSession {
public:
    std::function<void(std::string_view)> _on_log;
    httpd_handle_t _server = nullptr;
    std::unique_ptr<DnsServer> _dns_server;
    std::string _ssid;

    void log(std::string_view msg)
    {
        if (_on_log) {
            _on_log(msg);
        }
        ESP_LOGI(TAG, "%.*s", static_cast<int>(msg.size()), msg.data());
    }

    bool start()
    {
        if (!init_wifi_stack()) {
            log("WiFi stack initialization failed");
            return false;
        }

        esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (ap_netif == nullptr) {
            ap_netif = esp_netif_create_default_wifi_ap();
        }
        if (ap_netif == nullptr) {
            log("Failed to create AP netif");
            return false;
        }

        esp_netif_ip_info_t ip_info = {};
        IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        esp_netif_dhcps_stop(ap_netif);
        esp_netif_set_ip_info(ap_netif, &ip_info);
        esp_netif_dhcps_start(ap_netif);

        _ssid = make_ap_ssid();
        wifi_config_t wifi_cfg = {};
        std::memcpy(wifi_cfg.ap.ssid, _ssid.data(), std::min(_ssid.size(), sizeof(wifi_cfg.ap.ssid)));
        wifi_cfg.ap.ssid_len       = _ssid.size();
        wifi_cfg.ap.channel        = 1;
        wifi_cfg.ap.authmode       = WIFI_AUTH_OPEN;
        wifi_cfg.ap.max_connection = 4;

        esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_AP);
        esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
        esp_wifi_start();

        _dns_server = std::make_unique<DnsServer>();
        _dns_server->Start("192.168.4.1");

        httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
        config.stack_size       = 8192;
        config.max_uri_handlers = 18;
        config.uri_match_fn     = httpd_uri_match_wildcard;

        if (httpd_start(&_server, &config) != ESP_OK) {
            log("HTTP server start failed");
            return false;
        }

        register_uri("/", HTTP_GET, handle_index);
        register_uri("/index.html", HTTP_GET, handle_index);
        register_uri("/guruguru/state", HTTP_GET, handle_state);
        register_uri("/guruguru/image", HTTP_GET, handle_image);
        register_uri("/upload", HTTP_POST, handle_upload);
        register_uri("/guruguru/clear", HTTP_POST, handle_clear);
        register_uri("/exit", HTTP_POST, handle_exit);

        for (const auto* captive_url : _captive_portal_urls) {
            register_uri(captive_url, HTTP_GET, handle_captive_portal);
        }

        log("AP started! SSID: " + _ssid);
        return true;
    }

    void stop()
    {
        if (_server != nullptr) {
            httpd_stop(_server);
            _server = nullptr;
        }
        if (_dns_server) {
            _dns_server->Stop();
            _dns_server.reset();
        }
        wifi_config_t empty_ap = {};
        esp_wifi_set_config(WIFI_IF_AP, &empty_ap);
        esp_wifi_set_mode(WIFI_MODE_STA);
    }

    void register_uri(const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*))
    {
        httpd_uri_t route = {
            .uri      = uri,
            .method   = method,
            .handler  = handler,
            .user_ctx = this,
        };
        httpd_register_uri_handler(_server, &route);
    }

    static GuruguruApSession* self_from_request(httpd_req_t* req)
    {
        return static_cast<GuruguruApSession*>(req->user_ctx);
    }

    static esp_err_t handle_index(httpd_req_t* req)
    {
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_send(req, guruguru_config_ap_html_start,
                        guruguru_config_ap_html_end - guruguru_config_ap_html_start);
        return ESP_OK;
    }

    static esp_err_t handle_state(httpd_req_t* req)
    {
        auto* self = self_from_request(req);
        if (!self) return ESP_FAIL;

        std::string json = "{";
        json += "\"apSsid\":\"" + self->_ssid + "\",";
        json += "\"hasCustomAvatar\":" + std::string(GetHAL().hasGuruguruCustomAvatar() ? "true" : "false") + ",";
        json += "\"slices\":{";

        bool first = true;
        for (int i = 0; i < 9; ++i) {
            char openName[32];
            snprintf(openName, sizeof(openName), "dir%d.png", i);
            std::string openPath = std::string(_guru_dir) + "/" + openName;
            bool hasOpen = file_exists(openPath);

            if (!first) json += ",";
            first = false;
            json += "\"" + std::string(openName) + "\":" + (hasOpen ? "true" : "false");

            char closeName[32];
            snprintf(closeName, sizeof(closeName), "dir%d_close.png", i);
            std::string closePath = std::string(_guru_dir) + "/" + closeName;
            bool hasClose = file_exists(closePath);

            json += ",\"" + std::string(closeName) + "\":" + (hasClose ? "true" : "false");
        }
        json += "}}";

        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json.c_str(), json.size());
        return ESP_OK;
    }

    static esp_err_t handle_image(httpd_req_t* req)
    {
        char query[64] = {};
        if (httpd_req_get_url_query_len(req) <= 0 || httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing query");
            return ESP_FAIL;
        }

        char name[32] = {};
        if (httpd_query_key_value(query, "name", name, sizeof(name)) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing name");
            return ESP_FAIL;
        }

        std::string full_path = std::string(_guru_dir) + "/" + name;
        std::vector<uint8_t> data;
        if (!read_binary_file(full_path, data)) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "slice not found");
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "image/png");
        httpd_resp_send(req, reinterpret_cast<const char*>(data.data()), data.size());
        return ESP_OK;
    }

    static esp_err_t handle_upload(httpd_req_t* req)
    {
        auto* self = self_from_request(req);
        if (!self) return ESP_FAIL;

        if (req->content_len <= 0 || static_cast<std::size_t>(req->content_len) > _max_upload_size_bytes) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid upload size");
            return ESP_FAIL;
        }

        char query[64] = {};
        char filename[48] = {};
        if (httpd_req_get_url_query_len(req) > 0 && httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            httpd_query_key_value(query, "filename", filename, sizeof(filename));
        }

        if (filename[0] == '\0') {
            int hdr_len = httpd_req_get_hdr_value_len(req, "X-File-Name");
            if (hdr_len > 0 && hdr_len < static_cast<int>(sizeof(filename))) {
                httpd_req_get_hdr_value_str(req, "X-File-Name", filename, sizeof(filename));
            }
        }

        if (filename[0] == '\0') {
            snprintf(filename, sizeof(filename), "dir4.png");
        }

        ensure_guru_dir();
        std::string final_path = std::string(_guru_dir) + "/" + filename;
        std::string temp_path  = final_path + ".tmp";

        FILE* fp = fopen(temp_path.c_str(), "wb");
        if (!fp) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to create temp file");
            return ESP_FAIL;
        }

        std::size_t remaining = req->content_len;
        char buffer[1024];
        bool read_error = false;

        while (remaining > 0) {
            int to_read = std::min(remaining, sizeof(buffer));
            int recved = httpd_req_recv(req, buffer, to_read);
            if (recved <= 0) {
                read_error = true;
                break;
            }
            if (fwrite(buffer, 1, recved, fp) != static_cast<size_t>(recved)) {
                read_error = true;
                break;
            }
            remaining -= recved;
        }

        fclose(fp);

        if (read_error) {
            unlink(temp_path.c_str());
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write error");
            return ESP_FAIL;
        }

        unlink(final_path.c_str());
        rename(temp_path.c_str(), final_path.c_str());

        self->log(std::string("Uploaded: ") + filename);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"ok\"}", 15);
        return ESP_OK;
    }

    static esp_err_t handle_clear(httpd_req_t* req)
    {
        GetHAL().clearGuruguruCustomAvatar();
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"ok\"}", 15);
        return ESP_OK;
    }

    static esp_err_t handle_exit(httpd_req_t* req)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"ok\"}", 15);
        GetHAL().requestGuruguruUploadExit();
        return ESP_OK;
    }

    static esp_err_t handle_captive_portal(httpd_req_t* req)
    {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", _ap_url);
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, nullptr, 0);
        return ESP_OK;
    }
};

} // namespace

/* -------------------------------------------------------------------------- */
/*                                HAL Interface                               */
/* -------------------------------------------------------------------------- */
bool Hal::hasGuruguruCustomAvatar()
{
    ensure_guru_dir();
    // Center slice dir4.png is the anchor image
    std::string path_png = std::string(_guru_dir) + "/dir4.png";
    std::string path_jpg = std::string(_guru_dir) + "/dir4.jpg";
    return file_exists(path_png) || file_exists(path_jpg);
}

bool Hal::getGuruguruSlicePath(int dir, bool closed, std::string& path)
{
    if (dir < 0 || dir > 8) dir = 4;

    if (closed) {
        char close_file[48];
        snprintf(close_file, sizeof(close_file), "%s/dir%d_close.png", _guru_dir, dir);
        if (file_exists(close_file)) {
            // "A:" is the LVGL FS_STDIO prefix configured in config.json
            path = std::string("A:") + close_file;
            return true;
        }
    }

    char open_file[48];
    snprintf(open_file, sizeof(open_file), "%s/dir%d.png", _guru_dir, dir);
    if (file_exists(open_file)) {
        path = std::string("A:") + open_file;
        return true;
    }

    // Fallback to center dir 4 if this specific direction slice wasn't uploaded
    snprintf(open_file, sizeof(open_file), "%s/dir4.png", _guru_dir);
    if (file_exists(open_file)) {
        path = std::string("A:") + open_file;
        return true;
    }

    return false;
}

void Hal::clearGuruguruCustomAvatar()
{
    DIR* dir = opendir(_guru_dir);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
            std::string full_path = std::string(_guru_dir) + "/" + entry->d_name;
            unlink(full_path.c_str());
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "Cleared all custom avatar slices");
}

void Hal::startGuruguruUploadModeViaAp(std::function<void(std::string_view)> onLog)
{
    ESP_LOGI(TAG, "start Guruguru upload mode via AP");

    if (s_guru_event_group == nullptr) {
        s_guru_event_group = xEventGroupCreate();
    }
    xEventGroupClearBits(s_guru_event_group, _exit_bit);

    GuruguruApSession session;
    session._on_log = onLog;

    if (!session.start()) {
        if (onLog) onLog("Failed to start Guruguru AP session");
        return;
    }

    if (onLog) {
        std::string info = "WiFi: " + session._ssid + "\nURL: http://192.168.4.1\n\nWaiting for uploads...";
        onLog(info);
    }

    // Wait until exit requested (e.g. from physical home button or web finish)
    xEventGroupWaitBits(s_guru_event_group, _exit_bit, pdFALSE, pdTRUE, portMAX_DELAY);

    if (onLog) onLog("Stopping WiFi AP & saving...");
    session.stop();

    if (s_guru_event_group != nullptr) {
        vEventGroupDelete(s_guru_event_group);
        s_guru_event_group = nullptr;
    }

    ESP_LOGI(TAG, "Guruguru upload AP mode stopped");
}

void Hal::requestGuruguruUploadExit()
{
    if (s_guru_event_group != nullptr) {
        xEventGroupSetBits(s_guru_event_group, _exit_bit);
    }
}
