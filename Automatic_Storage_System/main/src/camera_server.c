#define BOARD_ESP32CAM_AITHINKER 1

#include <nvs_flash.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "camera_pins.h"
#include "wifi_connection.h"
#include <esp_log.h>
#include <stddef.h>
#include "face_detector.h"
#include <mbedtls/base64.h>
#include <time.h>
clock_t start, end;
// #define LOG_DEBUG
#define LOG_DEBUG_DETECT
#define COUNT_DETECT 6
static uint8_t detect_send[] = "face_detect";
size_t detect_send_len = sizeof(detect_send);
#define LED_GPIO_PIN 4 // GPIO 4 for the onboard LED
SemaphoreHandle_t g_handle_image = NULL;
camera_fb_t g_image;

static const char *TAG = "camera_server";
static const char *TAG1 = "IMAGE_WEBSOCKET";
static const char *TAG2 = "SIGNAL_WEBSOCKET";
static int camera_w = 0;
static int camera_h = 0;
esp_err_t image_process(httpd_req_t *req, httpd_ws_frame_t *pkt, uint8_t *count_face);
// Store wi-fi credentials
char ssid[32] = "Trinh";
char password[64] = "0333755401 ";

static esp_err_t init_camera(void) {
    camera_config_t camera_config = {
        .pin_pwdn  = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 10000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_RGB565,
        .frame_size = FRAMESIZE_QQVGA,

        .jpeg_quality = 17,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY
    };
    
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        return err;
    }

    // Get current camera resolution
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "Camera sensor not found");
        return ESP_FAIL;
    }
    get_camera_current_resolution(s->status.framesize, &camera_w, &camera_h);
    ESP_LOGI(TAG, "Current resolution: %dx%d", camera_w, camera_h);

    return ESP_OK;
}

// Initialize GPIO pins
esp_err_t init_pins(void) {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << LED_GPIO_PIN;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    return gpio_config(&io_conf);
}

// Handle requirements when receiving from WS client
esp_err_t handle_ws_req(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG1, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    ESP_LOGI(TAG1, "Received data from websocket");

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG1, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG1, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG1, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG1, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG1, "Got packet with message: %s", ws_pkt.payload);
    }
#ifdef LOG_DEBUG
	ESP_LOGI(TAG1, "Packet final: %d", ws_pkt.final);
	ESP_LOGI(TAG1, "Packet fragmented: %d", ws_pkt.fragmented);
	ESP_LOGI(TAG1, "Packet type: %d", ws_pkt.type);
#endif
    if ( strcmp((char*)ws_pkt.payload,"capture") == 0 )
    {
        uint8_t count_face = 0;
        while( (ret == ESP_OK) && (count_face <= COUNT_DETECT) )
        {
            ret = image_process(req, &ws_pkt, &count_face);
            //Switch to other task
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
	return ret;
}

// Websocket handler
httpd_uri_t ws_uri = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = handle_ws_req,
    .user_ctx = NULL,
    .is_websocket = true
};

httpd_handle_t setup_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t stream_httpd = NULL;

    // Start the httpd server and register handlers
    if (httpd_start(&stream_httpd , &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &ws_uri);
    }

    return stream_httpd;
}

void app_main() {
    esp_err_t err;

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // Connect to wifi
    connect_wifi(ssid, password);

	// Initialize camera
	err = init_camera();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to init camera: %s", esp_err_to_name(err));
		return;
	}

	// Initialize pins
	err = init_pins();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to init pins: %s", esp_err_to_name(err));
		return;
	}
	
	//Create mutex
	g_handle_image = xSemaphoreCreateMutex();
	// Start web server
	setup_server();
	ESP_LOGI(TAG, "ESP32 Web Camera Streaming Server is up and running");
}


esp_err_t image_process(httpd_req_t *req, httpd_ws_frame_t *pkt, uint8_t *count_face)
{
    esp_err_t ret = ESP_OK;
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG1, "Camera Capture Failed");
        return ESP_FAIL;
    }
    //Detect face
    bool detect = false;
    if (*count_face == COUNT_DETECT)
    {
        detect = face_detection((uint16_t*)fb->buf, (int)fb->width, (int)fb->height, 3);
    }
    else
        detect = inference_face_detection((uint16_t*)fb->buf, (int)fb->width, (int)fb->height, 3);

    //count face
    if ( (true == detect) && (*count_face < COUNT_DETECT) )
    {
        *count_face += 1;
        ESP_LOGI(TAG1, "count_face %d", *count_face);
    }
    else if ( (true == detect) && (*count_face == COUNT_DETECT) )
    {
        ESP_LOGI(TAG1, "count_face %d", *count_face);
        pkt->type = HTTPD_WS_TYPE_TEXT;
        pkt->payload = detect_send;
        pkt->len = detect_send_len;
#ifdef LOG_DEBUG_DETECT
        ESP_LOGI(TAG1, "detect ws_pkt.len: %d", pkt->len);
        ESP_LOGI(TAG1, "detect ws_pkt.type: %d", pkt->type);
        ESP_LOGI(TAG1, "detect ws_pkt.payload: %s", pkt->payload);
#endif
        ret = httpd_ws_send_frame(req, pkt);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG1, "httpd_ws_send_frame failed with %d", ret);
            return ret;
        }
        *count_face += 1;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    else
    {
        *count_face = 0;
    }

    //store value to ws_pkt
    pkt->payload = fb->buf;
    pkt->len = fb->len;
    pkt->type = HTTPD_WS_TYPE_BINARY;
#ifdef LOG_DEBUG
    ESP_LOGI(TAG1, "Packet pkt->len: %d", pkt->len);
    ESP_LOGI(TAG1, "Packet pkt->type: %d", pkt->type);
    ESP_LOGI(TAG1, "Packet pkt->fragmented: %d", pkt->fragmented);
    ESP_LOGI(TAG1, "Packet pkt->final: %d", pkt->final);
#endif
    esp_camera_fb_return(fb);

    // Send to WebSocket
    ret = httpd_ws_send_frame(req, pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG1, "httpd_ws_send_frame failed with %d", ret);
        return ret;
    }
    return ESP_OK;
}