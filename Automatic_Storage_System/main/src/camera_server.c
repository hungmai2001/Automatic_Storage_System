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
esp_err_t image_process(httpd_req_t *req, httpd_ws_frame_t *pkt, uint8_t *count_face, bool * fir_cap);
int get_sequence_number(const char *str, const char *pattern);
// Store wi-fi credentials
char ssid[32] = "Trinh";
char password[64] = "0333755401 ";


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
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG_MQTT, "sent publish successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG_MQTT, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG_MQTT, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG_MQTT, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG_MQTT, "sent publish successful, msg_id=%d", msg_id);
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

        .xclk_freq_hz = 10000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_RGB565,
        .frame_size = FRAMESIZE_HQVGA,

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
    //Receive "capture" message from web browser to send image
    if ( strcmp((char*)ws_pkt.payload,"capture") == 0 )
    {   
        bool first_cap = true;
        uint8_t count_face = 0;
        while( (ret == ESP_OK) && (count_face <= COUNT_DETECT) )
        {
            ret = image_process(req, &ws_pkt, &count_face, &first_cap);
            //Switch to other task
            vTaskDelay(pdMS_TO_TICKS(20));
        }
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


esp_err_t image_process(httpd_req_t *req, httpd_ws_frame_t *pkt, uint8_t *count_face, bool * fir_cap)
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
        ESP_LOGI(TAG1, "count_face %d", *count_face);
        *count_face += 1;
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
        ESP_LOGI(TAG2, "detected ws_pkt.final: %d", pkt->final);
        ESP_LOGI(TAG2, "detected ws_pkt.fragmented: %d", pkt->fragmented);
#endif
        ret = httpd_ws_send_frame(req, pkt);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG1, "httpd_ws_send_frame failed with %d", ret);
            return ret;
        }
        memset(pkt, 0, sizeof(httpd_ws_frame_t));
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
    memset(pkt, 0, sizeof(httpd_ws_frame_t));
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