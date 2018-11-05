#include "csiwriter.h"
#include <sys/ipc.h>
#include <errno.h>
#include <strings.h>
#include <sys/shm.h>
#include <stdio.h>

CSIWriter::CSIWriter(int mslots)
{
    index = 0;
    shmid_sem = -1;
    shmid_csi = -1;
    slots = mslots;
    sem_array = (sem_t*)-1;
    csi_array = (csi_packet*)-1;
    init_shm_put();
}

CSIWriter::~CSIWriter()
{
    if (sem_array != (void*)-1) {
        destroy_sems();
        shmdt(sem_array);
    }
    if (csi_array != (void*)-1) {
        shmdt(csi_array);
    }
    if (shmid_sem > 0) {
        shmctl(shmid_sem, IPC_RMID, nullptr);
    }
    if (shmid_csi > 0) {
        shmctl(shmid_csi, IPC_RMID, nullptr);
    }
}

bool CSIWriter::init_shm_put()
{
    key_t key_sem = ftok(SHM_SEM_TOKEN, 0);
    key_t key_csi = ftok(SHM_CSI_TOKEN, 0);
    // convert a global token to shm keys
    if (key_sem == -1 || key_csi == -1) {
        fprintf(stderr, "Error converting shm keys: %s\n", strerror(errno));
        return false;
    }
    // get shm segment for semaphores
    if ((shmid_sem = shmget(key_sem, sizeof(sem_t)*slots, SHM_MODE|IPC_CREAT|IPC_EXCL)) < 0) {
        fprintf(stderr, "Error allocating shared memory segments: %s\n", strerror(errno));
        return false;
    }
    sem_array = (sem_t*)shmat(shmid_sem, nullptr, 0);
    if (sem_array == (void*)-1) {
        fprintf(stderr, "Error attaching to shared meory segments: %s\n", strerror(errno));
        return false;
    }
    // get shm segment for csi buffer
    if ((shmid_csi = shmget(key_csi, sizeof(csi_packet)*slots, SHM_MODE|IPC_CREAT|IPC_EXCL)) < 0) {
        fprintf(stderr, "Error allocating shared memory segments: %s\n", strerror(errno));
        return false;
    }
    csi_array = (csi_packet*)shmat(shmid_csi, nullptr, 0);
    if (csi_array == (void*)-1) {
        fprintf(stderr, "Error attaching to shared meory segments: %s\n", strerror(errno));
        return false;
    }

    // initialize semaphore array
    for (int i = 0; i < slots; i++) {
        if (sem_init(sem_array+i, 1, 0) < 0) {
            fprintf(stderr, "Error initilizing semaphores: %s\n", strerror(errno));
            return false;
        }
    }
    return true;
}

bool CSIWriter::write_csi_to_shm(const csi_packet *packet)
{
    if (packet == nullptr) {
        fprintf(stderr, "Null pointer: No where to store packet\n");
        return false;
    }
    int value;
    while (true) {
        if (sem_getvalue(sem_array+index, &value) < 0) {
            fprintf(stderr, "Error calling sem_getvalue: %s\n", strerror(errno));
            return false;
        }
        if (value == 0)
            break;
    }
    memcpy(csi_array+index, packet, sizeof(csi_packet));
    while (true) {
        if (sem_post(sem_array+index) < 0) {
            if (errno == EINTR)
                continue;
            else
                return false;
        }
        break;
    }
    index = (index+1) % slots;
    return true;
}

void CSIWriter::destroy_sems()
{
    for (int i = 0; i < slots; i++)
        sem_destroy(sem_array+i);
}
