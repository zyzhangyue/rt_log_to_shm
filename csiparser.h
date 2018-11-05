#ifndef CSIPARSER_H
#define CSIPARSER_H

#include "csi_packet.h"

class CSIParser
{
private:
    uint8_t *data, *payload;
    uint8_t antenna_sel, Ntx, Nrx;
    uint16_t calc_len, len;

private:
    uint8_t perm[3], regu[3], *map;

private:
    bool read_bfee(csi_packet*, void*);
    csi_packet* read_bf_file(const char*, int32_t*);
    bool get_scaled_csi(csi_packet*);
    bool module_Scaled(csi_packet*, const int);
    void print_csi_packet(csi_packet*);
    double get_total_rss(csi_packet*);
    double get_total_csi(csi_packet*);

public:
    CSIParser();
    bool parse_csi_from_buffer(csi_packet*, uint8_t*);
    csi_packet* parse_csi_from_file(const char*, int*);
};

#endif // CSIPARSER_H
