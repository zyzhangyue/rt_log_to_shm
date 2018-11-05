#include <stdio.h>
#include <signal.h>
#include "csi_packet.h"
#include "csiwriter.h"
#include "csifetcher.h"

//#define CSI_FROM_FILE

static CSIWriter *writer;

void sigint_handler(int signo)
{
    if (writer != nullptr) {
        writer->~CSIWriter();
    }
    exit_program_err();
    exit(-signo);
}

int main()
{
#ifndef CSI_FROM_FILE

    int i = 0;
    csi_packet packet;
    if (!csi_driver_init()) {
        return -1;
    }
    fprintf(stderr, "raw_csi_driver_init succeed\n");

    writer = new CSIWriter(2);
    if (writer == nullptr) {
        return -1;
    }
    fprintf(stderr, "shared memory init succeed\n");

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    signal(SIGSEGV, sigint_handler);

    while (true) {
        i++;
        if (!get_csi_from_driver(&packet)) {
            break;
        }
        if (writer->write_csi_to_shm(&packet)) {
            break;
        }
        if (i % 200 == 0) {
            fprintf(stderr, "%d sec(s) elapsed: %u\n", i/200, packet.timestamp_low);
        }
    }

#else
    csi_packet *packet = nullptr;
    int count;
    int c;
    writer = new CSIWriter(2);
    if (writer == nullptr) {
        return -1;
    }
    fprintf(stderr, "shared memory init succeed\n");

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    signal(SIGSEGV, sigint_handler);

    packet = get_all_csi_from_file("./amaoa2_1.dat", &count);

    std::cout << "Total CSI: " << count << endl;

    while (true) {
        std::cout << ">>";
        std::cin >> c;
        if (c == -1)
            break;
        writer->write_csi_to_shm(packet+c);
    }
#endif
    return 0;
}
