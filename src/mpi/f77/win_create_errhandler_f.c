/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_WIN_CREATE_ERRHANDLER = mpi_win_create_errhandler_f
#pragma weak pmpi_win_create_errhandler = mpi_win_create_errhandler_f
#pragma weak pmpi_win_create_errhandler_ = mpi_win_create_errhandler_f
#pragma weak pmpi_win_create_errhandler__ = mpi_win_create_errhandler_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_WIN_CREATE_ERRHANDLER,
                           pmpi_win_create_errhandler,
                           pmpi_win_create_errhandler_,
                           pmpi_win_create_errhandler__,
                           pmpi_win_create_errhandler_f,
                           (MPI_Fint *function, MPI_Fint *errhandler, MPI_Fint *ierr),
                           (function, errhandler, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_WIN_CREATE_ERRHANDLER = mpi_win_create_errhandler_f
#pragma weak mpi_win_create_errhandler = mpi_win_create_errhandler_f
#pragma weak mpi_win_create_errhandler_ = mpi_win_create_errhandler_f
#pragma weak mpi_win_create_errhandler__ = mpi_win_create_errhandler_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_WIN_CREATE_ERRHANDLER,
                           mpi_win_create_errhandler,
                           mpi_win_create_errhandler_,
                           mpi_win_create_errhandler__,
                           mpi_win_create_errhandler_f,
                           (MPI_Fint *function, MPI_Fint *errhandler, MPI_Fint *ierr),
                           (function, errhandler, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/c/profile/defines.h"
#endif

void mpi_win_create_errhandler_f(MPI_Fint *function, MPI_Fint *errhandler, MPI_Fint *ierr)
{

}
