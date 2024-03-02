// Select desired board from the list below
// #define BOARD_WROVER_KIT 1
// #define BOARD_CAMERA_MODEL_ESP_EYE 1
#define BOARD_ESP32CAM_AITHINKER 1
// #define BOARD_CAMERA_MODEL_TTGO_T_JOURNAL 1

#include <nvs_flash.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#include "camera_pins.h"
#include "wifi_connection.h"
#include <esp_log.h>
#include <stddef.h>
#include "face_detector.h"
#include <mbedtls/base64.h>

#define LOG_DEBUG
// #define CAL_TIME

#ifdef CAL_TIME
#include <time.h>
clock_t start, end, start1, end1;
#endif

#define LED_GPIO_PIN 4 // GPIO 4 for the onboard LED
SemaphoreHandle_t g_handle_image = NULL;
camera_fb_t g_image;

static const char *TAG = "camera_server";
static int camera_w = 0;
static int camera_h = 0;

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
        .frame_size = FRAMESIZE_QVGA,

        .jpeg_quality = 12,
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

int32_t calcBase64EncodedSize(int origDataSize)
{
	// Number of blocks in 6-bit units (rounded up in 6-bit units)
	int32_t numBlocks6 = ((origDataSize * 8) + 5) / 6;
	// Number of blocks in units of 4 characters (rounded up in units of 4 characters)
	int32_t numBlocks4 = (numBlocks6 + 3) / 4;
	// Number of characters without line breaks
	int32_t numNetChars = numBlocks4 * 4;
	// Size considering line breaks every 76 characters (line breaks are "\ r \ n")
	return numNetChars;
}

esp_err_t Image2Base64(camera_fb_t g_image_buf, size_t base64_buffer_len, uint8_t * base64_buffer)
{
	// Convert from JPEG to BASE64
	size_t encord_len;
	mbedtls_base64_encode(base64_buffer, base64_buffer_len, &encord_len, g_image_buf.buf, g_image_buf.len);
#ifdef LOG_DEBUG
	ESP_LOGI(TAG, "mbedtls_base64_encode: encord_len=%d", encord_len);
#endif
	return ESP_OK;
}

// Handle requirements when receiving from WS client
esp_err_t handle_ws_req(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Received data from websocket");

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }
#ifdef LOG_DEBUG
	ESP_LOGI(TAG, "Packet final: %d", ws_pkt.final);
	ESP_LOGI(TAG, "Packet fragmented: %d", ws_pkt.fragmented);
	ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
#endif
    while(true) {
		// Get Image size
		xSemaphoreTake(g_handle_image, portMAX_DELAY);
#ifdef CAL_TIME
		start1 = clock();
#endif
		// Get Base64 size
		int32_t base64Size = calcBase64EncodedSize(g_image.len);
#ifdef LOG_DEBUG
		ESP_LOGI(TAG, "base64Size=%"PRIi32, base64Size);
#endif
		// Allocate Base64 buffer
		// You have to use calloc. It doesn't work with malloc.
		uint8_t *base64_buffer = NULL;
		size_t base64_buffer_len = base64Size + 1;
		base64_buffer = calloc(1, base64_buffer_len);
		if (base64_buffer == NULL) {
			ESP_LOGE(TAG, "calloc fail. base64_buffer_len %d", base64_buffer_len);
			return ESP_FAIL;
		}
		memset(base64_buffer, 0, base64_buffer_len);

		// Convert from Image to Base64
		ret = Image2Base64(g_image, base64_buffer_len, base64_buffer);
#ifdef LOG_DEBUG
		ESP_LOGI(TAG, "Image2Base64=%d", ret);
#endif
		if (ret != ESP_OK) {
			free(base64_buffer);
			return ret;
		}
		// Send by WebSocket
		ws_pkt.payload = g_image.buf;
		ws_pkt.len = g_image.len;
		ws_pkt.type = HTTPD_WS_TYPE_BINARY;
#ifdef LOG_DEBUG
		ESP_LOGI(TAG, "Packet ws_pkt.len: %d", ws_pkt.len);
		ESP_LOGI(TAG, "Packet ws_pkt.type: %d", ws_pkt.type);
		ESP_LOGI(TAG, "Packet ws_pkt.fragmented: %d", ws_pkt.fragmented);
		ESP_LOGI(TAG, "Packet ws_pkt.final: %d", ws_pkt.final);
#endif
#ifdef CAL_TIME_SEND
		start2 = clock();
#endif
		ret = httpd_ws_send_frame(req, &ws_pkt);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
		}
#ifdef CAL_TIME_SEND
		end2 = clock();
		double time_taken = ((double)(end2 - start2))/CLOCKS_PER_SEC; // in seconds
		ESP_LOGI(TAG, "took %f mseconds to execute",time_taken*1000);
#endif
		free(base64_buffer);
#ifdef CAL_TIME
		end1 = clock();
		double time_taken = ((double)(end1 - start1))/CLOCKS_PER_SEC; // in seconds
		ESP_LOGI(TAG, "WS: took %f mseconds to execute",time_taken*1000);
#endif
		xSemaphoreGive(g_handle_image);
		vTaskDelay(pdMS_TO_TICKS(2000));
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

	size_t _jpg_buf_len;
    uint8_t* _jpg_buf;
	while (true) {
		xSemaphoreTake(g_handle_image, portMAX_DELAY);
#ifdef CAL_TIME
		start = clock();
#endif
		// Save Picture to Local file
		camera_fb_t * fb = esp_camera_fb_get();
		if (!fb) {
			ESP_LOGE(TAG, "Camera Capture Failed");
			break;
		}
		ESP_LOGI(TAG, "Picture format=%d",fb->format);
		inference_face_detection((uint16_t*)fb->buf, (int)fb->width, (int)fb->height, 3);

		// // Convert to jpeg if needed
        // if(fb->format != PIXFORMAT_JPEG) {
        //     bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        //     if(!jpeg_converted) {
        //         ESP_LOGE(TAG, "JPEG compression failed");
		// 		// break;
        //     }
        // }
        // else {
        //     _jpg_buf_len = fb->len;
        //     _jpg_buf = fb->buf;
        // }
		// ESP_LOGI(TAG, "Convert format=%d",fb->format);
		g_image = *fb;
		// g_image.buf = _jpg_buf;
		// g_image.len = _jpg_buf_len;
		// Free captured data
        // if(fb->format != PIXFORMAT_JPEG){
        //     free(_jpg_buf);
        // }

#ifdef LOG_DEBUG
		ESP_LOGI(TAG, "pictureSize=%d",g_image.len);
		// ESP_LOGI(TAG, "pictureSize=%s",g_image.buf);
		// ESP_LOGI(TAG, "pictureSize=%d",g_image.width);
#endif
		//return the frame buffer back to the driver for reuse
		esp_camera_fb_return(fb);
#ifdef CAL_TIME
		end = clock();
		double time_taken = ((double)(end - start))/CLOCKS_PER_SEC; // in seconds
		ESP_LOGI(TAG, "MAIN: took %f mseconds to execute",time_taken*1000);
#endif
		xSemaphoreGive(g_handle_image);
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}
