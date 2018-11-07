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
    double timestamp_low;
    double csiI[3][30];
    double csiR[3][30];
};

#endif
