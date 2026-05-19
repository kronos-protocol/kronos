#include "kronos.h"


uint8_t krs_version_encode(uint8_t major, uint8_t minor, uint8_t patch) {
    return (uint8_t)(((major & 0x07u) << 5) | ((minor & 0x07u) << 2) | (patch & 0x03u));
}

void krs_version_decode(uint8_t version, uint8_t* major, uint8_t* minor, uint8_t* patch) {
    if (major) *major = (version >> 5) & 0x07u;
    if (minor) *minor = (version >> 2) & 0x07u;
    if (patch) *patch = version & 0x03u;
}
