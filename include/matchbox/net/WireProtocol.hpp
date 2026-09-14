#pragma once

#include <cstdint>

namespace matchbox::net {

// ---------------------------------------------------------------------------
// Matchbox TCP order-entry wire protocol.
//
// Every message is a fixed-size, packed binary struct. Each field uses an
// explicit-width type (uint8_t / int64_t) so the layout is identical on any
// compiler, and `#pragma pack` tells the compiler to insert no padding bytes
// between fields. That means the byte layout on the wire is exactly the
// struct layout in memory, so send/recv can copy the struct directly.
//
// Byte order: host order. Little-endian on x86 (your WSL2 box), and both the
// gateway and the client run on this machine, so the raw bytes match. If the
// two ends ever live on differently-endian machines this would need explicit
// encode/decode helpers — not needed for now.
// ---------------------------------------------------------------------------

// Message type identifiers, one byte each. Only NEW_ORDER exists today;
// CANCEL/MODIFY would be added here later.
enum class MessageType : uint8_t {
    NEW_ORDER     = 1,  // client -> gateway: submit a new limit order
    NEW_ORDER_ACK = 2,  // gateway -> client: confirmation with assigned id
};

// A single "new order" instruction on the wire.
//
// Packed layout (byte offset : size : field):
//     0 : 1 : type         (MessageType::NEW_ORDER)
//     1 : 1 : side         0 = BUY, 1 = SELL  (matches matchbox::Side values)
//     2 : 1 : timeInForce  0 = GTC, 1 = IOC, 2 = FOK, 3 = GTD (matches TimeInForce)
//     3 : 8 : price        (int64_t)
//    11 : 8 : quantity     (int64_t)
//  total : 19 bytes
#pragma pack(push, 1)
struct NewOrderMessage {
    uint8_t type;        // must be MessageType::NEW_ORDER
    uint8_t side;        // matchbox::Side: BUY = 0, SELL = 1
    uint8_t timeInForce; // matchbox::TimeInForce: GTC = 0, IOC = 1, FOK = 2, GTD = 3
    int64_t price;
    int64_t quantity;
};
#pragma pack(pop)

static_assert(sizeof(NewOrderMessage) == 19,
              "NewOrderMessage must stay packed at exactly 19 bytes");

// The gateway's reply. Sent after the order has been processed by the engine.
//
// Packed layout (byte offset : size : field):
//     0 : 1 : type         (MessageType::NEW_ORDER_ACK)
//     1 : 1 : success      1 = accepted, 0 = rejected
//     2 : 8 : orderId      id assigned by the engine (0 on rejection)
//  total : 10 bytes
#pragma pack(push, 1)
struct NewOrderAck {
    uint8_t type;     // MessageType::NEW_ORDER_ACK
    uint8_t success;  // 1 = accepted, 0 = rejected
    int64_t orderId;  // assigned by the engine (0 if rejected)
};
#pragma pack(pop)

static_assert(sizeof(NewOrderAck) == 10,
              "NewOrderAck must stay packed at exactly 10 bytes");

}  // namespace matchbox::net
