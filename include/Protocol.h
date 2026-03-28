#pragma once

#include <cstdint>

#pragma pack(push, 1)

namespace market_handler {

    struct MessageHeader {
        char message_type; // 'A' = Add, 'X' = Cancel, 'M' = Modify
    };

    struct AddOrderMsg {
        MessageHeader header;
        uint64_t order_id;
        uint32_t price;
        uint32_t quantity;
        char is_buy;       // 'B' = Buy, 'S' = Sell
    };

    struct CancelOrderMsg {
        MessageHeader header;
        uint64_t order_id;
    };

    struct ModifyOrderMsg {
        MessageHeader header;
        uint64_t order_id;
        uint32_t new_price;
        uint32_t new_quantity;
    };

    struct alignas(64) PacketPayload {
        uint64_t ingress_tsc;

        union {
            MessageHeader header;
            AddOrderMsg add_msg;
            CancelOrderMsg cancel_msg;
            ModifyOrderMsg modify_msg;

            uint8_t _padding[56];
        };
    };

}

#pragma pack(pop)
