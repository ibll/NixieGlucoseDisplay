#ifndef ICONS_H
#define ICONS_H

#include <cstdint>

namespace Icon {

inline constexpr uint32_t no_wifi[3] = {
    0xf430841,
    0x20e01480,
    0x80160260,
};
inline constexpr uint32_t wifi_good[3] = {
    0xf030c40,
    0x20f01080,
    0x106a064,
};
inline constexpr uint32_t wifi[3] = {
    0xf030c40,
    0x20f01080,
    0x60060,
};
inline constexpr uint32_t wifi1[3] = {
    0xf030c40,
    0x20000f01,
    0x8060060,
};
inline constexpr uint32_t wifi2[3] = {
    0xf030,
    0xc4020f01,
    0x8060060,
};
inline constexpr uint32_t wifi3[3] = {
    0xf030,
    0xc4f21080,
    0x60060,
};

}

#endif // ICONS_H