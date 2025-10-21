#include "lan_task.h" 

#include "shared_resources.h"
#include "udp_protocol.h"
#include "peer_manager.h"

#include <stddef.h>
#include <string.h>
#include <unistd.h> // Для close()
#include "sys/socket.h"
#include "arpa/inet.h"
#include "fcntl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_manager.h"
// #include "esp_netif.h"

static const char *TAG = "LAN_TASK";

extern QueueHandle_t main_queue_event;

static uint64_t my_sn; 
static char sender_name[16];

typedef struct {
    int sock;
    uint16_t port;
} lan_task_params_t;


static lan_task_params_t task_params;

static void lan_rx_task(void *pvParameters)
{
    static const char *TAG = "lan_Rx_task";

    xEventGroupWaitBits(system_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    // Крок 1: Отримайте дескриптор сокета з pvParameters шляхом перетворення типів.
    lan_task_params_t *params = (lan_task_params_t *)pvParameters;
    int sockfd = params->sock;

    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    // Крок 2: Підготуйте буфер для прийому даних та структури для адреси відправника.
    uint8_t rx_buffer[MAX_UDP_PACKET_SIZE];
    // Крок 3: Увійдіть у нескінченний цикл while(1).     
    static uint32_t last_received_seq = 0;
    ESP_LOGD(TAG, "RX Task: запущено, слухаємо сокет %d", sockfd);   

    int64_t start_time, elapsed_time; //cleanof

    while(1){
    // Крок 4: Всередині циклу викличте recvfrom() для блокуючого очікування пакета.
    // Не забудьте перевірити результат на помилки.
        ssize_t len = recvfrom(sockfd, rx_buffer, MAX_UDP_PACKET_SIZE, 0, (struct sockaddr *)&source_addr, &socklen);
       
        if (len >= sizeof(udp_packet_header_t)) {

            start_time = esp_timer_get_time();
            char* sender_ip = inet_ntoa(source_addr.sin_addr);
            ESP_LOGD(TAG, "Отримано %zd байт від %s", len, sender_ip);

            udp_packet_header_t* header = (udp_packet_header_t*)rx_buffer;
            // Крок 1: Перевірка "секретного стуку"
            if (header->magic_number != MY_MAGIC_NUMBER) {
                // Це не наш пакет, ігноруємо
                ESP_LOGW(TAG, "Це не наш пакет, ігноруємо");
                continue;
            }

            // Крок 2: Фільтрація власного ехо (важливий пункт!)
            if (header->sender_sn == my_sn) {
                // Ми отримали власний broadcast-пакет, ігноруємо
                ESP_LOGW(TAG, "Ми отримали власний broadcast-пакет, ігноруємо");
                continue;
            }

            // Крок 3: Сортування за типом пакета
            switch (header->type) {
                case UDP_PACKET_DISCOVERY_QUERY:
                    ESP_LOGD(TAG, "UDP_PACKET_DISCOVERY_QUERY");
                    // Це запит "хто тут?" від іншого пристрою.
                    // Обробляємо його: оновлюємо "адресну книгу" і відправляємо у відповідь UNICAST-пакет LAN_PACKET_DISCOVERY_RESPONSE.
                    // --- КОД ДЛЯ ВІДЛАДКИ ---
                    // Виводимо отриманий пакет у HEX-форматі для емуляції в Packet Sender
                    ESP_LOGD(TAG, "--- RAW QUERY PACKET [%zd bytes] ---", len);
                    for (ssize_t i = 0; i < len; i++) {
                        ESP_LOGD(TAG,"%02X ", rx_buffer[i]);
                    }                    
                    ESP_LOGD(TAG, "--- END OF RAW PACKET ---");
                    // --- КІНЕЦЬ КОДУ ДЛЯ ВІДЛАДКИ ---
                    peer_manager_update_peer(header->sender_sn, 
                                 header->sender_name, 
                                 source_addr.sin_addr.s_addr);

                    udp_packet_header_t response_header = {
                        .magic_number = MY_MAGIC_NUMBER ,        // Наш "секретний стук" (напр. 0xDEADBEEF)
                        .type = UDP_PACKET_DISCOVERY_RESPONSE,       // Тип пакета з enum вище
                        .sender_sn = my_sn                                
                    };

                    // response_header.sender_name = sender_name,
                    strncpy(response_header.sender_name, sender_name, sizeof(response_header.sender_name) - 1);
                    response_header.sender_name[sizeof(response_header.sender_name) - 1] = '\0'; // Гарантуємо нуль-термінацію

                    sendto(sockfd, &response_header, sizeof(response_header), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));           
                    break;
                
                case UDP_PACKET_DISCOVERY_RESPONSE:
                    ESP_LOGD(TAG, "Отримано UDP_PACKET_DISCOVERY_RESPONSE пакет переклички від SN: %llu, Name: %s", header->sender_sn, header->sender_name);
                    peer_manager_update_peer(header->sender_sn, 
                                 header->sender_name, 
                                 source_addr.sin_addr.s_addr);
                    break;

                case UDP_PACKET_AUDIO:
                    // TODO: Оновити логіку обробки аудіо-пакетів відповідно до нової структури.
                    // Наразі ця частина тимчасово вимкнена, поки ми тестуємо систему виявлення.
                    // ESP_LOGD(TAG, "Отримано аудіо-пакет, тимчасово ігнорується.");
                    // break;
                    ESP_LOGD(TAG, "UDP_PACKET_AUDIO");
                    uint8_t* playback_buffer;
                    if (xQueueReceive(shared_buffer_pool, &playback_buffer, pdMS_TO_TICKS(10)) == pdTRUE) {
                        
                        uint8_t* audio_payload = rx_buffer + sizeof(udp_packet_header_t);
                        size_t audio_len = len - sizeof(udp_packet_header_t);

                        memcpy(playback_buffer, audio_payload, audio_len);

                        audio_chunk_t chunk_to_play = { .buffer = playback_buffer, .len = audio_len };
                        xQueueSend(net_to_speaker_queue, &chunk_to_play, 0);

                        app_event_t ui_event = {
                            .source = EVENT_SOURCE_CHANNEL, // Використовуємо існуюче джерело
                            .CmdCode = CHANNEL_EVENT_PEER_SPEAKING, // Потрібно буде додати цей тип
                        };
                        ui_event.payload.channel.peer_speaking = header->sender_sn; // Передаємо SN мовця
                        xQueueSend(main_queue_event, &ui_event, 0);

                        if (header->sequence_number > last_received_seq + 1) {
                            ESP_LOGW(TAG, "Втрачено %lu аудіо-пакет(ів)!", header->sequence_number - last_received_seq - 1);
                        }
                        last_received_seq = header->sequence_number;

                    } else { 
                        ESP_LOGW(TAG, "Немає вільних буферів для відтворення, пакет пропущено!");
                    }
                    

                    elapsed_time = esp_timer_get_time() - start_time;
                    ESP_LOGI(TAG, "Функція Lan_rx()[%lld] виконувалася %lld мікросекунд, до [%lld]",start_time, elapsed_time,esp_timer_get_time());
                    break;
                
                case UDP_PACKET_AUDIO_END_TX:
                    ESP_LOGD(TAG, "UDP_PACKET_AUDIO_END_TX");
                    app_event_t ui_event = {
                            .source = EVENT_SOURCE_CHANNEL, // Використовуємо існуюче джерело
                            .CmdCode = CHANNEL_EVENT_PEER_SPEAKING_END, // Потрібно буде додати цей тип
                    };
                    ui_event.payload.channel.peer_speaking = header->sender_sn; // Передаємо SN мовця
                    xQueueSend(main_queue_event, &ui_event, 0);
                    break;
            }       
        }
    
    // (Поки що можна просто вивести в лог IP відправника та кількість отриманих байт).
    }
    ESP_LOGE(TAG, "ШОСЬ пішло не так у lan_rx_task");
    vTaskDelete(NULL);
}


static struct sockaddr_in g_tx_dest_addr;
SemaphoreHandle_t g_tx_dest_addr_mutex; //todo add to all g_tx_dest_addr

void lan_set_tx_target (peer_info_t *peer){ // todo mutex
    if (xSemaphoreTake(g_tx_dest_addr_mutex, portMAX_DELAY) == pdTRUE) {
        if (peer->sn)
            g_tx_dest_addr.sin_addr.s_addr = peer->ip_address;
        else
            g_tx_dest_addr.sin_addr.s_addr = INADDR_BROADCAST;
        // g_tx_dest_addr.sin_port = htons(peer->port); //todo for WAN
        ESP_LOGD(TAG, "g_tx_dest_addr_mutex changed to %lu", g_tx_dest_addr.sin_addr.s_addr);
        xSemaphoreGive(g_tx_dest_addr_mutex);
    }
     
}

static void lan_tx_task(void *pvParameters)
{
    static const char *TAG = "lan_Tx_task";

    xEventGroupWaitBits(system_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    // Крок 1: Отримайте дескриптор сокета з pvParameters.
    lan_task_params_t *params = (lan_task_params_t *)pvParameters;
    int sockfd = params->sock;

    ESP_LOGD(TAG, "TX Task: запущено, чекаємо xQueueReceive");   
    // Крок 3: Увійдіть у нескінченний цикл while(1).
    uint8_t tx_buffer[MAX_UDP_PACKET_SIZE]; 

    // Вказуємо на початок буфера як на заголовок
    udp_packet_header_t* header = (udp_packet_header_t*)tx_buffer;

    // Заповнюємо поля заголовка, які не змінюються в циклі
    header->magic_number = MY_MAGIC_NUMBER;
    header->type = UDP_PACKET_AUDIO;
    header->sender_sn = my_sn;
    strncpy(header->sender_name, sender_name, sizeof(header->sender_name) - 1);
    header->sender_name[sizeof(header->sender_name) - 1] = '\0'; // Гарантуємо нуль-термінацію

    audio_chunk_t received_chunk;

    static uint32_t packet_counter = 0;
    static bool last_packet = false;
    while(1){
    // Крок 4: Всередині циклу чекайте на дані з черги mic_to_net_queue за допомогою xQueueReceive().
        if (xQueueReceive(mic_to_net_queue, &received_chunk, pdMS_TO_TICKS(50))== pdTRUE) {

            memcpy(tx_buffer + sizeof(udp_packet_header_t), received_chunk.buffer, received_chunk.len);
            size_t total_packet_size = sizeof(udp_packet_header_t) + received_chunk.len;

            // Крок 5: Якщо дані отримано, відправте їх у мережу за допомогою sendto() на broadcast-адресу. //todo  Утримання м'ютекса протягом виконання блокуючої функції sendto() може призводити до затримок у реакції UI.
            // if (xSemaphoreTake(g_tx_dest_addr_mutex, portMAX_DELAY) == pdTRUE){
                header->sequence_number = packet_counter++; // Встановлюємо і збільшуємо лічильник

                ssize_t bytes_sent = sendto(sockfd, tx_buffer, total_packet_size, 0, (struct sockaddr *)&g_tx_dest_addr, sizeof(g_tx_dest_addr));
                if (bytes_sent < 0) {
                    ESP_LOGE(TAG, "Помилка відправки. Помилка: %d", errno);
                } else {
                    ESP_LOGD(TAG, "Успішно відправлено %zd байт", bytes_sent);
                }
            //     xSemaphoreGive(g_tx_dest_addr_mutex);
            // }
            last_packet = true;
            // Крок 6: Поверніть використаний аудіо-буфер назад у пул shared_buffer_pool.
            xQueueSend(shared_buffer_pool, &received_chunk.buffer, 0);
        }
        else if (last_packet) {
            header->type = UDP_PACKET_AUDIO_END_TX;
            ssize_t bytes_sent = sendto(sockfd, tx_buffer, sizeof(udp_packet_header_t), 0, (struct sockaddr *)&g_tx_dest_addr, sizeof(g_tx_dest_addr));
            ESP_LOGI(TAG, "UDP_PACKET_AUDIO_END_TX, bytes sent %d ",bytes_sent);
            last_packet = false;
            header->type = UDP_PACKET_AUDIO;
        }
    }
    ESP_LOGE(TAG, "ШОСЬ пішло не так у lan_tx_task");
    vTaskDelete(NULL);
}


static void lan_ping_task(void *pvParameters) {
    static const char *TAG = "lan_ping_task";

    xEventGroupWaitBits(system_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    // Тут буде логіка створення broadcast адреси, як у вашому lan_tx_task
    // ...
    ESP_LOGD(TAG, "lan_ping_task started");
    lan_task_params_t *params = (lan_task_params_t *)pvParameters;
    int sockfd = params->sock;
    uint16_t port = params->port;

    udp_packet_header_t udp_header = {
        .magic_number = MY_MAGIC_NUMBER ,        // Наш "секретний стук" (напр. 0xDEADBEEF)
        .type = UDP_PACKET_DISCOVERY_QUERY,       // Тип пакета з enum вище
        .sender_sn = my_sn
    };
     memcpy(udp_header.sender_name, sender_name, sizeof(sender_name));
    

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = INADDR_BROADCAST; // Адреса для відправки

    while(1) {
        // 1. Відправляємо broadcast-запит "хто тут?"
        //    Формуємо пакет типу UDP_PACKET_DISCOVERY_QUERY
        //    і відправляємо його через sendto()
        

        ssize_t bytes_sent = sendto(sockfd, &udp_header, sizeof(udp_packet_header_t), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        
        ESP_LOGD(TAG, "PING: UDP_PACKET_DISCOVERY_QUERY %d bytes sent",bytes_sent);
        // 2. Робимо очищення списку неактивних учасників
        peer_manager_cleanup_inactive();

        // peer_info_t local_list[MAX_PEERS];
        // int count = peer_manager_get_peers(local_list, MAX_PEERS);
        // ESP_LOGW(TAG, "======== СПИСОК УЧАСНИКІВ (%d) ========", count);
        // for (int i = 0; i < count; i++) {
        //     char ip_str[16];
        //     // esp_ip4_addr_t ip_addr = {.addr = local_list[i].ip_address};
        //     // esp_ip4addr_ntoa(&ip_addr, ip_str, sizeof(ip_str));
        //     inet_ntoa_r(local_list[i].ip_address, ip_str, sizeof(ip_str) - 1);
            
        //     ESP_LOGW(TAG, "  - %s (SN: %llu, IP: %s)", local_list[i].name, local_list[i].sn, ip_str);
        // }
        // ESP_LOGW(TAG, "========================================");
        // 3. Чекаємо N секунд перед наступною "перекличкою"
        vTaskDelay(pdMS_TO_TICKS(PEER_PING_PERIOD)); // Наприклад, кожні 15 секунд
    }
}


void lan_manager_init(int port)
{
    ESP_LOGD(TAG, "lan_manager_init START");
    // 
    
    g_tx_dest_addr_mutex = xSemaphoreCreateMutex();
    if (g_tx_dest_addr_mutex == NULL) {
        ESP_LOGE(TAG, "Не вдалося створити м'ютекс!");
        return; // Аварійне завершення
    }

    peer_manager_init(); 
    // Крок 1: Створіть UDP-сокет за допомогою socket(). Перевірте на помилки.
    int sockfd;

    uint8_t mac_addr[6];
    esp_read_mac(mac_addr, ESP_MAC_WIFI_STA);
    memcpy(&my_sn, mac_addr, sizeof(mac_addr));
    ESP_LOGD (TAG, " GOT stray SN = %llu", my_sn);

    strncpy(sender_name, nvs_manager_get_device_name(), sizeof(sender_name) - 1);
    sender_name[sizeof(sender_name) - 1] = '\0';
    ESP_LOGD (TAG, " GOT stray NAME = %s", sender_name);

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        ESP_LOGE(TAG, "Не вдалося створити сокет. Завершення задачі.");
        return;
    }
    // Крок 2: Налаштуйте сокет, щоб він міг відправляти broadcast-пакети (setsockopt з опцією SO_BROADCAST).
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        ESP_LOGE(TAG, "Не вдалося увімкнути broadcast. Помилка: %d", errno);
        close(sockfd);
        return;
    }
    ESP_LOGD(TAG, "Broadcast увімкнено.");
    // Крок 3: Прив'яжіть сокет до вашого LOCAL_PORT за допомогою bind(). Перевірте на помилки.
    struct sockaddr_in my_addr; 
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        ESP_LOGE(TAG, "Не вдалося прив'язати сокет. Завершення задачі.");
        close(sockfd);
        return;
    }
    ESP_LOGD(TAG, "Сокет успішно прив'язано до порту %d.", port);

    // Крок 4: ініціалізація адреси для відправки пакетів в lan_tx_task
    if (xSemaphoreTake(g_tx_dest_addr_mutex, portMAX_DELAY) == pdTRUE) {
    
        memset(&g_tx_dest_addr, 0, sizeof(g_tx_dest_addr));
        g_tx_dest_addr.sin_family = AF_INET;
        g_tx_dest_addr.sin_port = htons(port);
        g_tx_dest_addr.sin_addr.s_addr = INADDR_BROADCAST; 
        xSemaphoreGive(g_tx_dest_addr_mutex);
    }   

    // Крок 5: Створіть дві задачі (lan_rx_task, lan_tx_task) за допомогою xTaskCreate,
    // передавши їм дескриптор сокета як параметр pvParameters.
    task_params.sock = sockfd;
    task_params.port =(uint16_t )port;
    

    xTaskCreate(lan_rx_task, "lan_rx_task", 4096,  &task_params, 5, NULL);
    xTaskCreate(lan_tx_task, "lan_tx_task", 4096,  &task_params, 5, NULL);
    xTaskCreate(lan_ping_task, "lan_ping_task", 4096,  &task_params, 5, NULL);
    
    ESP_LOGD(TAG, "lan_manager_init ended!");
    xEventGroupSetBits(system_event_group, LAN_TASKS_INITIALIZED_BIT);
}




