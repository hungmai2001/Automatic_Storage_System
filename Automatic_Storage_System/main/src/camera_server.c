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
#include "mqtt_client.h"
#include <time.h>
clock_t start, end;
// #define LOG_DEBUG
#define LOG_DEBUG_DETECT
#define COUNT_DETECT 8
#define CONFIG_BROKER_URI "mqtt://mqtt.eclipseprojects.io"
static uint8_t detect_send[] = "face_detect";
size_t detect_send_len = sizeof(detect_send);

static uint8_t detected_send[] = "face_detected";
size_t detected_send_len = sizeof(detected_send);
static uint8_t no_detected_send[] = "no_face_detected";
size_t no_detected_send_len = sizeof(no_detected_send);
#define LED_GPIO_PIN 4 // GPIO 4 for the onboard LED
SemaphoreHandle_t g_handle_image = NULL;
camera_fb_t g_image;
const char *img_get_yes = "image_get_";
const char *img_get_no = "no_person";
const char *verify = "verify";
size_t len_recv_py_scripts = 0;
static const char *TAG = "camera_server";
static const char *TAG1 = "IMAGE_WEBSOCKET";
static const char *TAG2 = "IMAGE_GET";
static const char *TAG_MQTT = "mqttws_example";
static int camera_w = 0;
static int camera_h = 0;
esp_err_t image_process(httpd_req_t *req, httpd_ws_frame_t *pkt, bool * fir_cap);
int get_sequence_number(const char *str, const char *pattern);
bool downloaded_image = false;
// Store wi-fi credentials
char ssid[32] = "Hungmai";
char password[64] = "hungmaixx";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG_MQTT, "Last error %s: 0x%x", message, error_code);
    }
}
/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG_MQTT, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/topic/hungmai_wb_publish", 0);
        ESP_LOGI(TAG_MQTT, "sent subscribe successful, msg_id=%d", msg_id);
        // msg_id = esp_mqtt_client_subscribe(client, "/topic/hungmai_py_publish", 0);
        // ESP_LOGI(TAG_MQTT, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if (strstr(event->data, "downloaded_image") != NULL)
        {
            downloaded_image = true;
            ESP_LOGI(TAG_MQTT, "Downloaded_image");
        }
        // else if ( (strstr(event->data, "save_image_") != NULL) || (strstr(event->data, "image_detect") != NULL) || (strstr(event->data, "no_person_match") != NULL))
        // {
        //     downloaded_image = false;
        // }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG_MQTT, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG_MQTT, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URI,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

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

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,

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
	esp_err_t ret = mbedtls_base64_encode(base64_buffer, base64_buffer_len, &encord_len, g_image_buf.buf, g_image_buf.len);
	return ESP_OK;
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
    //Receive "capture" message from web browser to send image
    if ( strcmp((char*)ws_pkt.payload,"capture") == 0 )
    {
        downloaded_image = false;
        bool first_cap = true;
        ESP_LOGI(TAG1, "Start capture");
        while( (ret == ESP_OK) && (downloaded_image == false))
        {
            ret = image_process(req, &ws_pkt, &first_cap);
            //Switch to other task
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        ESP_LOGI(TAG1, "End capture");
        downloaded_image = false;
    }
    // Receive name of picture detected from python scripts
    else if (strstr((char*)ws_pkt.payload, img_get_yes) != NULL)
    {
        // Sử dụng hàm get_sequence_number để lấy số thứ tự sau "img_get_"
        int sequence_number = get_sequence_number((char*)ws_pkt.payload, img_get_yes);
        ESP_LOGI(TAG2, "Get number: %d", sequence_number);
        len_recv_py_scripts = detected_send_len;
    }
    // Receive "no_person" message from python scripts
    else if (strstr((char*)ws_pkt.payload, img_get_no) != NULL)
    {
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        len_recv_py_scripts = no_detected_send_len;
    }
    //Receive "verify" message from web browser
    else if (strcmp((char*)ws_pkt.payload,"verify") == 0 )
    {
        ESP_LOGI(TAG2, "len_recv_py_scripts: %d", len_recv_py_scripts);
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        ws_pkt.final = 0;
        ws_pkt.len = len_recv_py_scripts;
        if ( ws_pkt.len == detected_send_len )
        {
            ws_pkt.payload = detected_send;
        }
        else if ( ws_pkt.len == no_detected_send_len )
        {
            ws_pkt.payload = no_detected_send;
        }
        else
        {
            ESP_LOGE(TAG2, "Haven't received message from python scripts: %d", ws_pkt.len);
            /* Must handle additional this case */
            return ret;
        }
#ifdef LOG_DEBUG_DETECT
        ESP_LOGI(TAG2, "detected ws_pkt.type: %d", ws_pkt.type);
        ESP_LOGI(TAG2, "detected ws_pkt.payload: %s", ws_pkt.payload);
        ESP_LOGI(TAG2, "detected ws_pkt.final: %d", ws_pkt.final);
        ESP_LOGI(TAG2, "detected ws_pkt.fragmented: %d", ws_pkt.fragmented);
#endif
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG1, "httpd_ws_send_frame failed with %d", ret);
            return ret;
        }
        len_recv_py_scripts = 0;
        vTaskDelay(pdMS_TO_TICKS(20));
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
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    mqtt_app_start();
    
}


esp_err_t image_process(httpd_req_t *req, httpd_ws_frame_t *pkt, bool * fir_cap)
{
    esp_err_t ret = ESP_OK;
    //clear internal queue
    if ( *fir_cap == true)
    {
        for( int i = 0; i < 2 ;i++ ) {
            camera_fb_t * fb = esp_camera_fb_get();
            ESP_LOGI(TAG1, "fb->len=%d", fb->len);
            esp_camera_fb_return(fb);
        }
    }
    *fir_cap = false;
	//acquire a frame
	camera_fb_t * fb = esp_camera_fb_get();
	if (!fb) {
		ESP_LOGE(TAG1, "Camera Capture Failed");
		return ESP_FAIL;
	}
    camera_fb_t g_image = *fb;
    // Get Base64 size
    int32_t base64Size = calcBase64EncodedSize(g_image.len);

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
    if (ret != ESP_OK) {
        free(base64_buffer);
        return ret;
    }
    // Send by WebSocket
    pkt->type = HTTPD_WS_TYPE_TEXT;
    pkt->payload = base64_buffer;
    pkt->len = base64Size;
#ifdef LOG_DEBUG
    ESP_LOGI(TAG, "Packet ws_pkt.len: %d", ws_pkt.len);
    ESP_LOGI(TAG, "Packet ws_pkt.type: %d", ws_pkt.type);
    ESP_LOGI(TAG, "Packet ws_pkt.fragmented: %d", ws_pkt.fragmented);
    ESP_LOGI(TAG, "Packet ws_pkt.final: %d", ws_pkt.final);
#endif

    ret = httpd_ws_send_frame(req, pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
        return ret;
    }

    free(base64_buffer);
    esp_camera_fb_return(fb);
    return ESP_OK;
}

// Get number of string
int get_sequence_number(const char *str, const char *pattern)
{
    const char *number_start = strstr(str, pattern);
    if (number_start != NULL) 
    {
        //Move pointer to after pattern string
        number_start += strlen(pattern);
        // Convert to number
        char *endptr;
        long number = strtol(number_start, &endptr, 10);
        // Check status of conversion
        if (number_start != endptr)
        {
            return (int)number;
        }
    }
    return -1;
}