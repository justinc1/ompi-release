/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_COMM_REMOTE_SIZE = mpi_comm_remote_size_f
#pragma weak pmpi_comm_remote_size = mpi_comm_remote_size_f
#pragma weak pmpi_comm_remote_size_ = mpi_comm_remote_size_f
#pragma weak pmpi_comm_remote_size__ = mpi_comm_remote_size_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_COMM_REMOTE_SIZE,
                           pmpi_comm_remote_size,
                           pmpi_comm_remote_size_,
                           pmpi_comm_remote_size__,
                           pmpi_comm_remote_size_f,
                           (MPI_Fint *comm, MPI_Fint *size, MPI_Fint *ierr),
                           (comm, size, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_COMM_REMOTE_SIZE = mpi_comm_remote_size_f
#pragma weak mpi_comm_remote_size = mpi_comm_remote_size_f
#pragma weak mpi_comm_remote_size_ = mpi_comm_remote_size_f
#pragma weak mpi_comm_remote_size__ = mpi_comm_remote_size_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_COMM_REMOTE_SIZE,
                           mpi_comm_remote_size,
                           mpi_comm_remote_size_,
                           mpi_comm_remote_size__,
                           mpi_comm_remote_size_f,
                           (MPI_Fint *comm, MPI_Fint *size, MPI_Fint *ierr),
                           (comm, size, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/c/profile/defines.h"
#endif

void mpi_comm_remote_size_f(MPI_Fint *comm, MPI_Fint *size, MPI_Fint *ierr)
{

}
