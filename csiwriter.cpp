#include "csiwriter.h"
#include <sys/sem.h>
#include <sys/ipc.h>
#include <errno.h>
#include <strings.h>
#include <sys/shm.h>
#include <stdio.h>

CSIWriter::CSIWriter(int mslots)
{
    index = 0;
    semid = -1;
    shmid = -1;
    slots = mslots;
    csi_array = (csi_packet*)-1;
    init_shm_put();
}

CSIWriter::~CSIWriter()
{
    if (csi_array != (void*)-1) {
        shmdt(csi_array);
    }
    if (semid > 0) {
        semctl(semid, 0, IPC_RMID);
    }
    if (shmid > 0) {
        shmctl(shmid, IPC_RMID, nullptr);
    }
}

bool CSIWriter::init_shm_put()
{
    // convert a global token to XSI keys
    key_t key_sem = ftok(SHM_SEM_TOKEN, 0);
    key_t key_csi = ftok(SHM_CSI_TOKEN, 0);    
    if (key_sem == -1 || key_csi == -1) {
        fprintf(stderr, "Error converting shm keys: %s\n", strerror(errno));
        return false;
    }

sem_retry:
    // trying to reference to an existing semaphore set
    if ((semid = semget(key_sem, 0, SHM_MODE)) < 0) {
        // if no such a semaphore set exists, we allocate a new one
        if (errno == EINTR)
            goto sem_retry;
        if (errno != ENOENT)
            return false;
        // allocating a new XSI semaphore set
        if ((semid = semget(key_sem, slots, SHM_MODE|IPC_CREAT|IPC_EXCL)) < 0) {
            fprintf(stderr, "init_shm_put: %s\n", strerror(errno));
            return false;
        }
    }
    // initializing XSI semaphores
    unsigned short *values = (unsigned short*)calloc(slots, sizeof(unsigned short));
    if (semctl(semid, 0, SETALL, values) < 0) {
        fprintf(stderr, "init_shm_put: %s\n", strerror(errno));
        return false;
    }
    free(values);
    fprintf(stderr, "XSI semaphore set initialized\n");

shm_retry:
    // trying to referencing to an existing shared memory
    if ((shmid = shmget(key_csi, 0, SHM_MODE)) < 0) {
        if (errno == EINTR)
            goto shm_retry;
        if (errno != ENOENT)
            return false;
        // allocating a new shared memory segment for csi buffer
        if ((shmid = shmget(key_csi, sizeof(csi_packet)*slots, SHM_MODE|IPC_CREAT|IPC_EXCL)) < 0) {
            fprintf(stderr, "Error allocating shared memory segments: %s\n", strerror(errno));
            return false;
        }
    }
    // attach to shared memory
    csi_array = (csi_packet*)shmat(shmid, nullptr, 0);
    if (csi_array == (void*)-1) {
        fprintf(stderr, "Error attaching to shared meory segments: %s\n", strerror(errno));
        return false;
    }
    fprintf(stderr, "XSI shared memory initialized\n");

    return true;
}

bool CSIWriter::write_csi_to_shm(const csi_packet *packet)
{
    if (packet == nullptr) {
        fprintf(stderr, "Null pointer: No where to store packet\n");
        return false;
    }
    struct sembuf op;
    op.sem_num = index;
    op.sem_op = 0;
    op.sem_flg = 0;

    // waiting for semaphore to be 0
    if (semop(semid, &op, 1) < 0) {
        fprintf(stderr, "write_csi_to_shm: %s\n", strerror(errno));
        return false;
    }

    // put the csi to csi_array buffer
    memcpy(csi_array+index, packet, sizeof(csi_packet));

    // increment semaphore by 2
    op.sem_op = 2;
    if (semop(semid, &op, 1) < 0) {
        fprintf(stderr, "write_csi_to_shm: %s\n", strerror(errno));
        return false;
    }

    // update index
    index = (index+1) % slots;
    return true;
}
