/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"

#include <esp_http_server.h>

#include "pwm.h"

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "led_test";



/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

static const httpd_uri_t uri_favicon_ico = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_get_handler,
    .user_ctx  = NULL
};


/* An HTTP GET handler */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    // Get handle to embedded color selection script
    extern const unsigned char color_sel_start[] asm("_binary_color_sel_html_start");
    extern const unsigned char color_sel_end[]   asm("_binary_color_sel_html_end");
    const size_t color_sel_size = (color_sel_end - color_sel_start);

    // Add file upload form and script which on execution sends a POST request to /upload
    httpd_resp_send(req, (const char *)color_sel_start, color_sel_size);

    return ESP_OK;
}

static const httpd_uri_t uri_slash = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_get_handler,
    .user_ctx  = NULL
};
static const httpd_uri_t uri_index = {
    .uri       = "/index",
    .method    = HTTP_GET,
    .handler   = index_get_handler,
    .user_ctx  = NULL
};
static const httpd_uri_t uri_index_html = {
    .uri       = "/index.html",
    .method    = HTTP_GET,
    .handler   = index_get_handler,
    .user_ctx  = NULL
};

#define NIBBLE(n, val) \
        if (n >= '0' && n <= '9') {val = n-'0';} \
        else if (n >= 'A' && n <= 'F') {val = n-'A'+10;} \
        else if (n >= 'a' && n <= 'f') {val = n-'a'+10;} \
        else return -1;

int hexToInt(char hi_nibble, char lo_nibble, uint8_t *value) {
    int hi, lo;
    NIBBLE(hi_nibble, hi);
    NIBBLE(lo_nibble, lo);
    *value = hi << 4 | lo;
    return 0;
}

int parse_color_request(char* buf, uint8_t *led_nb, uint8_t *red, uint8_t *green, uint8_t *blue) {
    if (strncmp("led=", buf, 4)) return ESP_FAIL;
    buf += 4;
    char *ptr = buf;
    while (*ptr != '\n' && (ptr-buf) < 32) {ptr++;}
    *ptr = 0;
//     ESP_LOGI(TAG, "DBG parse led NB : %s", buf);
    *led_nb = atoi(buf);

    buf = ptr+1;
    if (strncmp("color=#", buf, 7)) return ESP_FAIL;
    buf += 7;

    if (hexToInt(*(buf), *(buf+1), red)) {
        ESP_LOGI(TAG, "Error parsing red");
        return ESP_FAIL;
    }
    if (hexToInt(*(buf+2), *(buf+3), green)) {
        ESP_LOGI(TAG, "Error parsing green");
        return ESP_FAIL;
    }
    if (hexToInt(*(buf+4), *(buf+5), blue)) {
        ESP_LOGI(TAG, "Error parsing blue");
        return ESP_FAIL;
    }
    return ESP_OK;
}


/* An HTTP POST handler */
static esp_err_t color_post_handler(httpd_req_t *req)
{
    char buf[100];
    uint8_t led_nb, red, green, blue;
    int ret = HTTPD_SOCK_ERR_TIMEOUT;

    if (req->content_len > 30) {
        ESP_LOGW(TAG, "Got a request with an obviously too long payload (%d bytes)", req->content_len);
        httpd_resp_send_err(req, 413, NULL);
    }

    while ((ret = httpd_req_recv(req, buf, sizeof(buf))) == HTTPD_SOCK_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "Request timeout, retrying...");
    }

    if (ret != req->content_len) {
        ESP_LOGE(TAG, "BAD REQUEST. content length is %d, but read %d bytes",
                 req->content_len, ret);
        httpd_resp_send_err(req, 400, NULL);
        return ESP_FAIL;
    }

    if (parse_color_request(buf, &led_nb, &red, &green, &blue) != ESP_OK) {
        ESP_LOGW(TAG, "Parse error, rejecting request");
        httpd_resp_send_err(req, 400, NULL);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "request parsed. led: %u, color: rgb(%u, %u, %u)", led_nb, red, green, blue);
    espflam_set_led_RGB(led_nb, red, green, blue);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static const httpd_uri_t uri_color = {
    .uri       = "/color",
    .method    = HTTP_POST,
    .handler   = color_post_handler,
    .user_ctx  = NULL
};


static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_favicon_ico);
        httpd_register_uri_handler(server, &uri_slash);
        httpd_register_uri_handler(server, &uri_index);
        httpd_register_uri_handler(server, &uri_index_html);
        httpd_register_uri_handler(server, &uri_color);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


led_conf_t led_config[3] = {
    {   .red_pin = 4,
        .green_pin = 16,
        .blue_pin = 17
    },
    {   .red_pin = 18,
        .green_pin = 19,
        .blue_pin = 21
    },
    {   .red_pin = 22,
        .green_pin = 23,
        .blue_pin = 25
    }
};

void app_main()
{
    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    espflam_leds_init(led_config);

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET


    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
}
