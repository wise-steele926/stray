// #include "udp_protocol.h"

#ifndef UDP_PROT_H
#define UDP_PROT_H

#include <stdint.h>
#include "shared_resources.h"

#define MY_MAGIC_NUMBER 0xDEADBEEF 
#define MAX_UDP_PACKET_SIZE (sizeof(udp_packet_header_t) + AUDIO_BUFFER_SIZE)

// Типи пакетів, які можуть ходити в нашій LAN-мережі
typedef enum {
    UDP_PACKET_DISCOVERY_QUERY,   // Запит "хто тут?"
    UDP_PACKET_DISCOVERY_RESPONSE,// Відповідь "я тут!"
    UDP_PACKET_AUDIO,             // Пакет з аудіо-даними
    UDP_PACKET_AUDIO_END_TX,      // Фінальний пакет, що сигналізує кінець передачі
} udp_packet_type_t;

// Наш єдиний заголовок для всіх LAN-пакетів
typedef struct {
    uint32_t magic_number;        // Наш "секретний стук" (напр. 0xDEADBEEF)
    udp_packet_type_t type;       // Тип пакета з enum вище
    uint64_t sender_sn;           // Унікальний серійний номер відправника
    char sender_name[16];         // Ім'я відправника для відображення в UI
    uint32_t sequence_number; // Порядковий номер пакета
} udp_packet_header_t;

typedef struct {
    udp_packet_type_t header;
    uint8_t audio_data[AUDIO_BUFFER_SIZE];
} udp_audio_packet_t;





#endif