#ifndef CSIFETCHER_H
#define CSIFETCHER_H

#include "csi_packet.h"

bool csi_driver_init(void);
bool get_csi_from_driver(csi_packet*);
void exit_program_err(void);
csi_packet* get_all_csi_from_file(const char*, int*);

#endif // CSIWRITER_H
