#ifndef CSIWRITER_H
#define CSIWRITER_H

#include "csi_packet.h"
#include <semaphore.h>

/*
 * user R/W
 * group R/W
 * others R/W
 */
#define SHM_MODE 0600
#define SHM_SEM_TOKEN "/home/zhangyue/sem.shm"
#define SHM_CSI_TOKEN "/home/zhangyue/csi.shm"

class CSIWriter
{
private:
    int slots, index;
    int shmid_sem, shmid_csi;
    sem_t *sem_array;
    csi_packet *csi_array;

private:
    bool init_shm_put();
    void destroy_sems();

public:
    CSIWriter(int slots = 2);
    ~CSIWriter();
    bool write_csi_to_shm(const csi_packet*);
};

#endif // CSIWRITER_H
