/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "freertos/portmacro.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "espwifi_handler.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>
#include "cJSON.h"
static const char *TAG = "MQTT_EXAMPLE";
char dataVal[10];
static esp_mqtt_client_handle_t mqtt_client;
#define ECHO_TEST_TXD (CONFIG_APP_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_APP_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)
#define MQTT_APP_CLIENT (CONFIG_BROKER_URL)
#define ECHO_UART_PORT_NUM      (CONFIG_APP_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE     (CONFIG_APP_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE    (CONFIG_APP_TASK_STACK_SIZE)
#define NUM_RECORDS 100
#define MQTT_CONNECTED_BIT BIT0
#define MQTT_FAIL_BIT      BIT1
#define BUF_SIZE (2024)
static EventGroupHandle_t mqtt_event_group;
const int MQTTCONNECTED_BIT = BIT2;
const int MQTT_MSG_PROCESS_BIT=BIT3;
static void log_error_if_nonzero(const char * message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            xEventGroupSetBits(mqtt_event_group, MQTTCONNECTED_BIT);
            xEventGroupSetBits(mqtt_event_group,MQTT_MSG_PROCESS_BIT);
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

            msg_id = esp_mqtt_client_subscribe(client, "/gokul/data_msg", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		xEventGroupClearBits(mqtt_event_group, MQTTCONNECTED_BIT);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            printf("%i",strncmp(event->topic,"/gokul/data_msg",event->topic_len));
            printf("%i",event->topic_len);
            if(!strncmp(event->topic, "/gokul/data_msg",event->topic_len)){
                        ESP_LOGI(TAG,"message published in topic");
                        xEventGroupClearBits(mqtt_event_group,MQTT_MSG_PROCESS_BIT);
                        //strcpy(dataVal,(char *)event->data);
                        //printf("%s",dataVal); 
                        printf("%i",strncmp(event->data,"off",event->data_len));
                        printf("%i",event->data_len);
                        if(strncmp(event->data,"off",event->data_len)==0){

                            ESP_LOGI(TAG,"sending OFF");
                            uart_write_bytes(ECHO_UART_PORT_NUM, "SC2 \r\n",7);
                        }
                        if (!strncmp(event->data,"on",event->data_len)) {
                            ESP_LOGI(TAG,"sending ON");
                            uart_write_bytes(ECHO_UART_PORT_NUM,"SB1 \r\n",7 );
                        }
                        
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        xEventGroupSetBits(mqtt_event_group,MQTT_MSG_PROCESS_BIT);
                    }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);
}
static void uart_app_intit(){

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));
}
static void echo_task(void *params)
{
    // Configure a temporary buffer for the incoming data and proccesing data
    char *data = (char *) malloc(BUF_SIZE);
    while (1) {

	      xEventGroupWaitBits(mqtt_event_group, MQTT_MSG_PROCESS_BIT, false, true, portMAX_DELAY);
        ESP_LOGI("uart loop","requesting uart data");
        char *jsonString = (char*)malloc(20*sizeof(char));
        cJSON *root;
        root = cJSON_CreateObject();
        uart_write_bytes(ECHO_UART_PORT_NUM,"SA0 \r\n",7 );
        // Read data from the UART
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, BUF_SIZE, 20 / portTICK_RATE_MS);
        if (len!=0) {
            char *raw_data = (char*)malloc(2*sizeof(char));
            raw_data=strtok(data,"V#" );
            int i = 0;
            char *p = strtok (raw_data, ";");
            //free(raw_data);
            char *array[8];
            //            free(raw_data);
            while (p != NULL)
            {
                array[i++] = p;
                p = strtok (NULL, ";");
            }
            //adding the datas to json file
            cJSON_AddStringToObject(root, "input_voltage",array[0]);
            cJSON_AddStringToObject(root, "output_voltage",array[1]);
            cJSON_AddStringToObject(root, "battery_voltage",array[2]);
            cJSON_AddStringToObject(root, "load_percentage",array[3]);
            cJSON_AddStringToObject(root, "solarStatus",array[4]);
            cJSON_AddStringToObject(root, "mainStatus",array[5]);
            cJSON_AddStringToObject(root, "batteryStatus",array[6]);
            cJSON_AddStringToObject(root, "inverterStatus",array[7]);
            jsonString=cJSON_Print(root);
            ESP_LOGI("json","%s",jsonString);
            xEventGroupWaitBits(mqtt_event_group,MQTTCONNECTED_BIT, false,true ,portMAX_DELAY );
            esp_mqtt_client_publish(mqtt_client,"/gokul/data" ,jsonString,strlen(jsonString), 0,0);
            free(jsonString);
            cJSON_Delete(root);
            root=NULL;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ESP_LOGI("heap","%i",esp_get_free_heap_size());
            

            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}
void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    mqtt_event_group = xEventGroupCreate();
    wifi_init_sta();
    mqtt_app_start();
    uart_app_intit();
	xEventGroupWaitBits(mqtt_event_group, MQTTCONNECTED_BIT, false, true, portMAX_DELAY);
  printf("creating task");
  xTaskCreatePinnedToCore(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE,NULL, 10, NULL,1);
}
