#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "camera_pins.h"

// ตั้งค่า WiFi
#define WIFI_SSID "Home_2.4G"
#define WIFI_PASS "11112222"

static httpd_handle_t camera_httpd = NULL;

void wifi_init_sta() {
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    printf("Connecting to WiFi...\n");
    while (esp_wifi_connect() != ESP_OK) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        printf(".");
    }
    printf("\nConnected to WiFi\n");
}

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char *part_buf[64];
    
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK) return res;
    
    while(true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            printf("Camera capture failed\n");
            res = ESP_FAIL;
            break;
        }
        
        size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        if(res != ESP_OK) break;
        
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if(res != ESP_OK) break;
        
        esp_camera_fb_return(fb);
        fb = NULL;
    }
    
    if(fb) esp_camera_fb_return(fb);
    return res;
}

void app_main() {
    // เริ่มต้น NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // ตั้งค่า WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_init_sta();
    
    // ตั้งค่ากล้อง
    camera_config_t config = {
        .pin_pwdn = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_sccb_sda = SIOD_GPIO_NUM,
        .pin_sccb_scl = SIOC_GPIO_NUM,
        .pin_d7 = Y9_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d0 = Y2_GPIO_NUM,
        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_UXGA,
        .jpeg_quality = 10,
        .fb_count = 2,
        .grab_mode = CAMERA_GRAB_LATEST
    };
    
    // เริ่มต้นกล้อง
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        printf("Camera init failed with error 0x%x\n", err);
        return;
    }
    
    // ตั้งค่าเซ็นเซอร์
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 0);  // Flip ภาพแนวตั้ง
    s->set_hmirror(s, 1); // Mirror ภาพแนวนอน
    
    // เริ่ม HTTP Server
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };
    
    if (httpd_start(&camera_httpd, &http_config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &stream_uri);
        printf("HTTP server started\n");
    }
    
    printf("Camera Stream Ready! Go to: http://%s/stream\n", 
           esp_netif_ip4toa(&((esp_netif_ip_info_t *)esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")))->ip));
}
