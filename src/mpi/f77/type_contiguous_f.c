/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_TYPE_CONTIGUOUS = mpi_type_contiguous_f
#pragma weak pmpi_type_contiguous = mpi_type_contiguous_f
#pragma weak pmpi_type_contiguous_ = mpi_type_contiguous_f
#pragma weak pmpi_type_contiguous__ = mpi_type_contiguous_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_TYPE_CONTIGUOUS,
                           pmpi_type_contiguous,
                           pmpi_type_contiguous_,
                           pmpi_type_contiguous__,
                           pmpi_type_contiguous_f,
                           (MPI_Fint *count, MPI_Fint *oldtype, MPI_Fint *newtype, MPI_Fint *ierr),
                           (count, oldtype, newtype, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_TYPE_CONTIGUOUS = mpi_type_contiguous_f
#pragma weak mpi_type_contiguous = mpi_type_contiguous_f
#pragma weak mpi_type_contiguous_ = mpi_type_contiguous_f
#pragma weak mpi_type_contiguous__ = mpi_type_contiguous_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_TYPE_CONTIGUOUS,
                           mpi_type_contiguous,
                           mpi_type_contiguous_,
                           mpi_type_contiguous__,
                           mpi_type_contiguous_f,
                           (MPI_Fint *count, MPI_Fint *oldtype, MPI_Fint *newtype, MPI_Fint *ierr),
                           (count, oldtype, newtype, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/c/profile/defines.h"
#endif

void mpi_type_contiguous_f(MPI_Fint *count, MPI_Fint *oldtype, MPI_Fint *newtype, MPI_Fint *ierr)
{

}
