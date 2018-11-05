#ifndef CSI_PACKET_H
#define CSI_PACKET_H

#include <stdint.h>
#include <armadillo>

using namespace arma;

struct csi_packet {
    uint8_t Nrx, Ntx;
    uint8_t rssi_a, rssi_b, rssi_c;
    int8_t noise;
    uint8_t agc;
    uint8_t perm[3];
    uint16_t bfee_count, rate;
    uint32_t timestamp_low;
    double csiI[30][3];
    double csiR[30][3];
};

#endif
