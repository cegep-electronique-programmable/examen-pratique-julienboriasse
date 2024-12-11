#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_IDF";
#define WIFI_SSID "iphone"
#define WIFI_PASS "boriasse"

// Déclaration de la variable MQTT_CONNECTED qui permet de savoir si le client MQTT est connecté au broker MQTT
uint32_t MQTT_CONNECTED = 0;

// Déclaration de la fonction de démarrage du client MQTT definie plus bas
static void mqtt_app_start(void);

// Fonction de gestion des événements Wi-Fi
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    // Si le Wi-Fi est démarré, on se connecte au point d'accès Wi-Fi
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG, "Tentative de connexion Wi-Fi\n");
        break;

    // Si le Wi-Fi est connecté, on affiche un message
    case SYSTEM_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "Connexion Wi-Fi reussie!\n");
        break;

    // Si le Wi-Fi est connecté, on affiche l'adresse IP attribuée par le point d'accès Wi-Fi
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "Adresse IP obtenue. Demarrage du client MQTT\n");
        mqtt_app_start();
        break;

    // Si le Wi-Fi est déconnecté, on tente de se reconnecter
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "Wi-Fi perdu, tentative de reconnexion Wi-Fi\n");
        esp_wifi_connect();
        break;

    default:
        break;
    }
    return ESP_OK;
}

// Fonction d'initialisation du Wi-Fi
void wifi_init(void)
{
    // Initialisation
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Création de l'interface Wi-Fi
    esp_netif_create_default_wifi_sta();

    // Configuration du Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Enregistrement de la fonction de gestion des événements Wi-Fi
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    // Configuration du SSID et du mot de passe du Wi-Fi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    // Démarrage du Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Fonction de gestion des événements MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Evenement MQTT : base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    // Si le client MQTT est connecté, on s'abonne à des topics
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        MQTT_CONNECTED = 1;

        msg_id = esp_mqtt_client_subscribe(client, "/cm/test1", 0);
        ESP_LOGI(TAG, "Abonnement au topic /cm/test1 avec succes, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/cm/test2", 1);
        ESP_LOGI(TAG, "Abonnement au topic /cm/test2 avec succes, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/tge/e0000000/sub", 1);
        ESP_LOGI(TAG, "Abonnement au topic /tge/e0000000/sub avec succes, msg_id=%d", msg_id);
        break;

    // Si le client MQTT est déconnecté, on se désabonne de topics
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        MQTT_CONNECTED = 0;
        break;

    // Si le client MQTT s'abonne à une topic
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    // Si le client MQTT se desabonne d'une topic
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    // Si le client MQTT publie un message
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    // Si le client MQTT reçoit un message
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;

    // Si le client MQTT rencontre une erreur
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    
    // Si le client MQTT rencontre un événement inconnu
    default:
        ESP_LOGI(TAG, "Autre evenement id:%d", event->event_id);
        break;
    }
}

// Déclaration du client MQTT
esp_mqtt_client_handle_t client = NULL;

// Fonction de démarrage du client MQTT
static void mqtt_app_start(void)
{
    // Configuration du client MQTT
    ESP_LOGI(TAG, "Demarrage MQTT");
    esp_mqtt_client_config_t mqttConfig = {
        .uri = "mqtt://broker.hivemq.com:1883"};

    // Initialisation du client MQTT
    client = esp_mqtt_client_init(&mqttConfig);

    // Enregistrement de la fonction de gestion des événements MQTT
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);

    // Démarrage du client MQTT
    esp_mqtt_client_start(client);
}

// Tâche Publisher_Task
void Publisher_Task(void *params)
{
    while (true)
    {
        // Si la connexion au broker MQTT est bonne, on publie un message
        if (MQTT_CONNECTED)
        {
            ESP_LOGI(TAG, "Publication d'un message sur le topic /cm/test3");
            esp_mqtt_client_publish(client, "/cm/test3", "Message du microcontrôleur", 0, 0, 0);
            // On attend 15 secondes avant de publier un nouveau message
            vTaskDelay(15000 / portTICK_PERIOD_MS);
        }
    }
}

// Cette fonction est équivalente à la fonction main() en C ou en C++ standard
void app_main(void)
{
    // Initialisation de la mémoire non volatile
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialisation du Wi-Fi
    wifi_init();

    // Création de la tâche Publisher_Task
    xTaskCreate(Publisher_Task, "Publisher_Task", 1024 * 5, NULL, 5, NULL);
}