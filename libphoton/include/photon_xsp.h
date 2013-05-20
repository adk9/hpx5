#ifndef PHOTON_XSP_H
#define PHOTON_XSP_H

#include <netinet/in.h>
#include <mpi.h>

/* This must be synchronized with xsp/include/option_types.h */
#define PHOTON_CI     0x00
#define PHOTON_RI     0x01
#define PHOTON_SI     0x02
#define PHOTON_FI     0x03
#define PHOTON_IO     0x04
#define PHOTON_MIN    PHOTON_CI
#define PHOTON_MAX    PHOTON_IO

#define SLAB_INFO     0x04
#define SLAB_MIN      SLAB_INFO
#define SLAB_MAX      SLAB_INFO


typedef struct photon_ledger_info {
    uint32_t rkey;
    uintptr_t va;
} PhotonLedgerInfo;

/*
 * The actual XSP message for PhotonIOInfo must have variable size.
 * This is the current format (size = length = int):
 *
 * |size_of_fileURI|fileURI...|amode|niter|combiner|nints|integers...|
 * |naddrs|addresses...|ndatatypes|datatypes...|
 */

enum PhotonMPITypes {
    PHOTON_MPI_DOUBLE = 52,
};

typedef struct photon_mpi_datatype {
    int combiner;
    int nints;
    int *integers;
    int naddrs;
    MPI_Aint *addresses;
    int ndatatypes;
    int *datatypes;
} PhotonMPIDatatype;

typedef struct photon_io_info {
    char *fileURI;
    int amode;
    int niter;
    PhotonMPIDatatype view;
} PhotonIOInfo;


int photon_xsp_init(int nproc, int myrank, MPI_Comm comm, int *phorwarder);
int photon_xsp_init_server(int nproc);
int photon_xsp_phorwarder_io_init(char *fileURI, int amode, MPI_Datatype view, int niter);
int photon_xsp_phorwarder_io_finalize();

#endif
