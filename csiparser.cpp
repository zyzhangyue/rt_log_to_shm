#include "csiparser.h"
#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include <arpa/inet.h>
#include <stdlib.h>

#define dbinv(x) (pow(10.0, ((double)(x))/10.0))

static uint8_t triangle[4] = {0, 1, 3, 6};
static uint8_t buffer[1024];

CSIParser::CSIParser()
{
    data = payload = nullptr;
    Nrx = Ntx = 0;
    antenna_sel = 0;
    len = calc_len = 0;
    perm[0] = 0, perm[1] = 0, perm[2] = 0;
    regu[0] = 1, regu[1] = 2, regu[2] = 3;
    map = regu;
}

void CSIParser::print_csi_packet(csi_packet *packet)
{
    fprintf(stderr, "csi_packet:\n{\n");
    fprintf(stderr, "    timestamp: %u\n", packet->timestamp_low);
    fprintf(stderr, "    bfee_count: %u\n", packet->bfee_count);
    fprintf(stderr, "    Ntx: %u\n", packet->Ntx);
    fprintf(stderr, "    Nrx: %u\n", packet->Nrx);
    fprintf(stderr, "    rssi_a: %u\n", packet->rssi_a);
    fprintf(stderr, "    rssi_b: %u\n", packet->rssi_b);
    fprintf(stderr, "    rssi_c: %u\n", packet->rssi_c);
    fprintf(stderr, "    noise: %d\n", packet->noise);
    fprintf(stderr, "    agc: %u\n", packet->agc);
    fprintf(stderr, "    perm: [%u %u %u]\n", packet->perm[0], packet->perm[1], packet->perm[2]);
    fprintf(stderr, "    rate: %u\n", packet->rate);
    fprintf(stderr, "    csi:\n");
    Cube<double> *csiI = new Cube<double>((double*)packet->csiI, 1, 3, 30, false, false);
    Cube<double> *csiR = new Cube<double>((double*)packet->csiR, 1, 3, 30, false, false);
    Cube<cx_double> csi(*csiR, *csiI);
    std::cout << csi;
    fprintf(stderr, "}\n");
    delete csiI;
    delete csiR;
}

/* The computational routine */
bool CSIParser::read_bfee(csi_packet *packet, void *buffer)
{
    data = (uint8_t*)buffer;
    if (packet == nullptr || data == nullptr)
        return false;
    packet->timestamp_low = ((uint32_t*)data)[0];
    packet->bfee_count = ((uint16_t*)data)[2];
    packet->Nrx = Nrx = data[8];
    packet->Ntx = Ntx = data[9];
    packet->rssi_a = data[10];
    packet->rssi_b = data[11];
    packet->rssi_c = data[12];
    packet->noise = data[13];
    packet->agc = data[14];
    packet->rate = ((uint16_t*)data)[9];

    antenna_sel = data[15];
    len = ((uint16_t*)data)[8];
    calc_len = (30 * (Nrx * Ntx * 8 * 2 + 3) + 7) / 8;
    payload = &data[20];

    int8_t tmp;
    uint32_t j, k, index, remainder;
    packet->perm[0] = perm[0] = ((antenna_sel) & 0x3) + 1;
    packet->perm[1] = perm[1] = ((antenna_sel >> 2) & 0x3) + 1;
    packet->perm[2] = perm[2] = ((antenna_sel >> 4) & 0x3) + 1;

    /* Check that length matches what it should */
    if (len != calc_len) {
        fprintf(stderr, "MIMOToolbox:read_bfee_new:size Wrong beamforming matrix size.\n");
        return false;
    }

    index = 0;
    if ((perm[0]+perm[1]+perm[2]) == triangle[Nrx]) {
        map = perm;
    }
    /* Compute CSI from all this crap :) */
    for (k = 0; k < 30; k++) {
        index += 3;
        remainder = index % 8;

        for (j = 0; j < Nrx; j++) {
            tmp = (payload[index / 8] >> remainder)|(payload[index/8+1] << (8-remainder));
            packet->csiR[map[j]-1][k] = (double)tmp;
            tmp = (payload[index / 8+1] >> remainder)|(payload[index/8+2] << (8-remainder));
            packet->csiI[map[j]-1][k] = (double)tmp;
            index += 16;
        }
    }
    return true;
}

bool CSIParser::parse_csi_from_buffer(csi_packet *packet, uint8_t *buffer)
{
    int field_len = 0, broken_perm = 0;

    if (packet == nullptr || buffer == nullptr) {
        fprintf(stderr, "parse_csi_from_buffer: packet nullptr\n");
        return false;
    }

    field_len = ((uint16_t *)buffer)[0];

    if (buffer[2] != 187 || field_len != 213) {
        fprintf(stderr, "parse_csi_from_buffer: wrong csi format\n");
        return false;
    }

    if (!read_bfee(packet, buffer+3)) {
        fprintf(stderr, "parse_csi_from_buffer: error parsing csi packet\n");
        return false;
    }

    uint8_t *perm = packet->perm;
    uint8_t Nrx = packet->Nrx;
    if ((perm[0] + perm[1] + perm[2]) != triangle[Nrx]) {   //matrix does not contain default values
        if (broken_perm == 0) {
            broken_perm = 1;
            fprintf(stderr, "WARN ONCE: Found CSI with Nrx=%d and invalid perm\n", Nrx);
        }
    }
    return module_Scaled(packet, 1);
}

csi_packet* CSIParser::parse_csi_from_file(const char* filename, int* count)
{
    csi_packet *packet;
    packet = read_bf_file(filename, count);
    std::cout << "field:" << endl;
    if (!module_Scaled(packet, *count)) {
        return nullptr;
    }
    return packet;
}

bool CSIParser::module_Scaled(csi_packet *packet, const int count)
{
    if (packet == nullptr || count == 0) {
        fprintf(stderr, "Error in Module_scaled: no packet\n");
        return false;
    }
    for (int i = 0; i < count; i++) {
        if (!get_scaled_csi(packet+i)) {
            fprintf(stderr, "Error in Module_scaled: calling get_scaled_csi failed\n");
            return false;
        }
    }
    return true;
}

bool CSIParser::get_scaled_csi(csi_packet *packet)
{
    double csi_pwr = get_total_csi(packet);
    double rssi_pwr = dbinv(get_total_rss(packet));
    double scale = rssi_pwr / (csi_pwr / 30.0);
    double noise_db = (packet->noise == -127) ? -92.0 : (double)packet->noise;
    double thermal_noise_pwr = dbinv(noise_db);
    double quant_error_pwr = scale * (packet->Nrx * packet->Ntx);
    double total_noise_pwr = thermal_noise_pwr + quant_error_pwr;

    scale = sqrt(scale / total_noise_pwr);
    for (int i = 0 ; i < 3; i++) {
        for (int j = 0; j < 30; j++) {
            packet->csiI[i][j] *= scale;
            packet->csiR[i][j] *= scale;
        }
    }
    return true;
}

double CSIParser::get_total_rss(csi_packet *packet)
{
    double rssi_mag = 0.0;
    if (packet->rssi_a != 0) {
        rssi_mag += dbinv((packet->rssi_a));
    }
    if (packet->rssi_b != 0) {
        rssi_mag += dbinv((packet->rssi_b));
    }
    if (packet->rssi_c != 0) {
        rssi_mag += dbinv((packet->rssi_c));
    }
    return 10.0*log10(rssi_mag) -44.0 - (double)packet->agc;
}

double CSIParser::get_total_csi(csi_packet *packet)
{
    double *csiI = (double*)packet->csiI;
    double *csiR = (double*)packet->csiR;
    double ret = 0.0;
    for (int i = 0; i < 30*3*1; i++) {
        ret += csiI[i]*csiI[i] + csiR[i]*csiR[i];
    }
    return ret;
}

csi_packet* CSIParser::read_bf_file(const char* filename, int32_t *count)
{
    csi_packet *packet;
    long len, num;
    long cur = 0, broken_perm = 0, field_len = 0;

    if (filename == nullptr || count == nullptr) {
        fprintf(stderr, "Error no valid parameters\n");
        return nullptr;
    }
    FILE *file = fopen(filename, "rb");
    if (file == nullptr) {
        fprintf(stderr, "Error opening file %s: %s\n", filename, strerror(errno));
        return nullptr;
    }
    if (fseek(file, 0, SEEK_END) < 0) {
        fprintf(stderr, "Error seeking the end of file %s: %s\n", filename, strerror(errno));
        goto falseend;
    }
    len = ftell(file);
    if (fseek(file, 0, SEEK_SET) < 0) {
        fprintf(stderr, "Error seeking the start of file %s: %s\n", filename, strerror(errno));
        goto falseend;
    }
    *count = len / 215;
    if (len % 215 > 0) {
        fprintf(stderr, "File %s has broken csi packets\n", filename);
        goto falseend;
    }

    packet = (csi_packet*)calloc(*count, sizeof(csi_packet));
    num = -1;
    while (cur < len-3) {
        if (fread(buffer+0, 3, 1, file) != 1) {
            fprintf(stderr, "Error reading record from %s: %s\n", filename, strerror(errno));
            goto falseend;
        }
        field_len = htons(((uint16_t *)buffer)[0]);
        cur += field_len + 2;
        if (buffer[2] == 187) {
            if (fread(buffer+3, field_len-1, 1, file) != 1) {
                fprintf(stderr, "Error reading record from %s: %s\n", filename, strerror(errno));
                goto falseend;
            }
        }
        else {
            fseek(file, field_len-1, SEEK_CUR);
            continue;
        }

        if (buffer[2] == 187) { //hex2dec('bb')) Beamforming matrix -- output a record
            num ++;
            if (!read_bfee(packet+num, buffer+3)) {
                fprintf(stderr, "Error parsing CSI packet\n");
                goto falseend;
            }
            uint8_t *perm = packet[num].perm;
            uint8_t Nrx = packet[num].Nrx;
            if (Nrx == 1) { //No permuting needed for only 1 antenna
                continue;
            }
            if ((perm[0] + perm[1] + perm[2]) != triangle[Nrx]) {   //matrix does not contain default values
                if (broken_perm == 0) {
                    broken_perm = 1;
                    fprintf(stderr, "WARN ONCE: Found CSI %s with Nrx=%d and invalid perm\n", filename, Nrx);
                }
            }
        }
    }
    fclose(file);
    return packet;;
falseend:
    fclose(file);
    return nullptr;
}
