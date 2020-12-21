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
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"

#include <esp_http_server.h>

#include "pwm.h"
#include "mode_mngt.h"
#include "color_spaces.h"

#include <cJSON.h>


#include "temp_config.h"



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
        return ESP_FAIL;
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





// TODO: clean that Doubleplusugly chunk : i copied those private struct from mode_mngt.c
typedef struct s_candle {
    float yellowishity;
    float intensity;
} candle_t;

typedef struct s_cb_fire_data {
    uint64_t next_timestamp;
    candle_t candle[3];
} cb_fire_data_t;

cb_fire_data_t fire_arg;

extern void cb_mode_fire( void * arg );

//*****

static esp_err_t fire_post_handler(httpd_req_t *req)
{
    int ret = HTTPD_SOCK_ERR_TIMEOUT;
    if (mode_config.handle != NULL) {
        esp_timer_stop(mode_config.handle);
        mode_config.handle = NULL;
    }
    char *buf = malloc(req->content_len+2);
    if (! buf) {
        ESP_LOGE(TAG, "fire_post_handler() mem alloc for handling request failed.");
        httpd_resp_send_err(req, 500, "Not enought memory");
        return ESP_FAIL;
    }

    while ((ret = httpd_req_recv(req, buf, req->content_len)) == HTTPD_SOCK_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "fire_post_handler() Request timeout, retrying...");
    }

    if (ret != req->content_len) {
        ESP_LOGE(TAG, "BAD REQUEST. content length is %d, but read %d bytes (%s)",
                req->content_len, ret, buf);
        httpd_resp_send_err(req, 400, "Content-Length mismatch actual length");
        return ESP_FAIL;
    }

    cJSON *jsonreq = cJSON_Parse(buf);
    mode_config.mode = FIRE;
    mode_config.config.fire.brightness = cJSON_GetObjectItem(jsonreq,"brightness")->valuedouble;
    mode_config.config.fire.flickering = cJSON_GetObjectItem(jsonreq,"flickering")->valuedouble;
    mode_config.config.fire.speed = cJSON_GetObjectItem(jsonreq,"speed")->valuedouble;

    cJSON_Delete(jsonreq);
    free(buf);

    ESP_LOGI(TAG, "Got a /fire request : {B: %.4f, F: %.4f, S: %.4f}",
                mode_config.config.fire.brightness,
                mode_config.config.fire.flickering,
                mode_config.config.fire.speed);

    fire_arg.next_timestamp = 0;

    for (uint i=0; i<3; i++) {
        fire_arg.candle[i].intensity = 0.5;
        fire_arg.candle[i].yellowishity = 0.5;
    }

    esp_timer_create_args_t create_args = {
        .callback = cb_mode_fire,
        .arg = &fire_arg,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fireFX"
    };

    uint timer_period = 10000;
#if DEBUG_FIRE == 1
    #warning "Fire FX Debug mode"
    //more time for trace printing
    timer_period *= 10;
#endif
    if (esp_timer_create(&create_args,  &mode_config.handle) != ESP_OK
        || esp_timer_start_periodic(mode_config.handle, timer_period) != ESP_OK) {
        httpd_resp_send_err(req, 500, "Could not start task to handle fire effect");
    }

    httpd_resp_sendstr(req, NULL);
    return ESP_OK;
}

static const httpd_uri_t uri_fire = {
    .uri       = "/fire",
    .method    = HTTP_POST,
    .handler   = fire_post_handler,
    .user_ctx  = NULL
};



static esp_err_t firestop_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /firestop");
    if (mode_config.handle != NULL) {
        esp_timer_stop(mode_config.handle);
        ESP_LOGI(TAG, "Timer stopped");
        mode_config.handle = NULL;
        httpd_resp_sendstr(req, NULL);
        ESP_LOGI(TAG, "200 OK sent");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Fire FX not running...");
    httpd_resp_send_err(req, 400, "Fire FX not running");
    return ESP_FAIL;
}


static const httpd_uri_t uri_firestop = {
    .uri       = "/firestop",
    .method    = HTTP_POST,
    .handler   = firestop_post_handler,
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
        httpd_register_uri_handler(server, &uri_fire);
        httpd_register_uri_handler(server, &uri_firestop);
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


esp_err_t string2ip(const char *ipstr, struct esp_ip4_addr *ip) {
    char * tobetokenizen = strdup(ipstr);
    int ip_ints[4];

    ESP_LOGI(TAG, "Parse IP string : '%s'", ipstr);
    char *token = strtok(tobetokenizen, ".");
    for(int i=0; i<4; i++) {
        if (token==NULL) {
            ESP_LOGE(TAG, "Token #%d not found !", i);
            goto string2ip_parse_err;
        }
        ip_ints[i] = atoi(token);
        if (ip_ints[i]<0 || ip_ints[i]>255) {
            ESP_LOGE(TAG, "Token #%d (%d) not in 0..255 range", i, ip_ints[i]);
            goto string2ip_parse_err;
        }
        token = strtok(NULL, ".");
    }
    // check there is nothing trailing....
    if (strtok(NULL, ".") != NULL) {
        ESP_LOGE(TAG, "More than 4 tokens !");
        goto string2ip_parse_err;
    }

    ip->addr = ESP_IP4TOADDR(ip_ints[0], ip_ints[1], ip_ints[2], ip_ints[3]);
    free(tobetokenizen);
    return ESP_OK;

string2ip_parse_err:
    free(tobetokenizen);
    return ESP_FAIL;
}




// TODO: clean that Doubleplusugly chunk : i copied those private struct from mode_mngt.c
typedef enum e_bootanim_step {
    BOOTANIM_INIT=0,
    DIMMUP,
    FADE,
    ROTATION,
    DIMMDOWN
} bootanim_step_t;


typedef struct s_cb_bootanim_data {
    bootanim_step_t step;
    HSV_color_t led1_hsv;
    HSV_color_t led2_hsv;
    HSV_color_t led3_hsv;
    uint64_t ref_time;
} cb_bootanim_data_t;

extern void cb_mode_bootanim(void* arg);
//*****



void app_main()
{
    static httpd_handle_t server = NULL;

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *netif;

    espflam_leds_init(led_config);

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
#warning "Compiling WiFi FW"
    netif = esp_netif_create_default_wifi_sta();
    #ifdef CONFIG_EXAMPLE_CONNECT_STATIC
        esp_netif_dhcpc_stop(netif);
        //set the static IP
        string2ip(CONFIG_EXAMPLE_WIFI_STATIC_IP, &ipInfo.ip);
        ip4addr_aton(CONFIG_EXAMPLE_WIFI_STATIC_GW, &ipInfo.gw);
        ip4addr_aton(CONFIG_EXAMPLE_WIFI_STATIC_NETMASK, &ipInfo.netmask);
        ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ipInfo));
    #endif

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

#endif // CONFIG_EXAMPLE_CONNECT_WIFI




#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
#warning "Compiling ETH FW"
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    netif = esp_netif_new(&cfg);
    #ifdef CONFIG_EXAMPLE_CONNECT_STATIC
        // Set default handlers to process TCP/IP stuffs
        ESP_ERROR_CHECK(esp_eth_set_default_handlers(netif));
    #endif //CONFIG_EXAMPLE_CONNECT_STATIC

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));

#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET


#ifdef CONFIG_EXAMPLE_CONNECT_STATIC
    esp_netif_dhcpc_stop(netif);
    //set the static IP
    string2ip(CONFIG_EXAMPLE_WIFI_STATIC_IP, &ipInfo.ip);
    string2ip(CONFIG_EXAMPLE_WIFI_STATIC_GW, &ipInfo.gw);
    string2ip(CONFIG_EXAMPLE_WIFI_STATIC_NETMASK, &ipInfo.netmask);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ipInfo));
#endif

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());


    // Launch boot animation

    mode_config.mode = BOOTANIM;
    uint timer_period = 10000;

    cb_bootanim_data_t ba_data;
    ba_data.step = BOOTANIM_INIT;

    esp_timer_create_args_t create_args = {
        .callback = cb_mode_bootanim,
        .arg = &ba_data,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "bootanim"
    };

    if (esp_timer_create(&create_args,  &mode_config.handle) != ESP_OK
        || esp_timer_start_periodic(mode_config.handle, timer_period) != ESP_OK) {
            ESP_LOGE("app_main", "Could not start boot animation task");
    } else {
        vTaskDelay(30000/portTICK_PERIOD_MS);
        ESP_LOGI("app_main", "Stopping boot animation timer");
        esp_timer_stop(mode_config.handle);
    }
}
