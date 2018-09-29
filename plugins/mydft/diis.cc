/*
 *@BEGIN LICENSE
 *
 * mydft, a plugin to:
 *
 * Psi4: an open-source quantum chemistry software package
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (c) 2014, The Florida State University. All rights reserved.
 * 
 *@END LICENSE
 *
 */

#include "diis.h"
#include "psi4/libqt/qt.h"
#include "psi4/libpsio/psio.hpp"

#include "psi4/psifiles.h"

#include <string.h>

using namespace psi;

namespace psi{ namespace mydft {

DIIS::DIIS(int n) {

    dimdiis_           = n;
    maxdiis_           = 8;
    diis_iter_         = 0;
    replace_diis_iter_ = 1;
    diisvec_           = (double*)malloc(sizeof(double)*(maxdiis_+1));
    tmp1_              = (double*)malloc(2*dimdiis_*sizeof(double));
    tmp2_              = (double*)malloc(2*dimdiis_*sizeof(double));

}
DIIS::~DIIS() {
    free(diisvec_);
    free(tmp1_);
    free(tmp2_);
}

// Write current solution vector to disk (in file PSIF_DCC_OVEC).
void DIIS::WriteVector(double * vector1, double * vector2){

    // Name the entry in PSIF_DCC_OVEC according to the current
    // DIIS iteration.  If we already have maxdiis_ vectors, then
    // replace one.
    char * oldvector = (char*)malloc(1000*sizeof(char));
    if ( diis_iter_ <= maxdiis_ ){
       sprintf(oldvector,"oldvector%i",diis_iter_);
    }
    else{
       sprintf(oldvector,"oldvector%i",replace_diis_iter_);
    }

    std::shared_ptr<PSIO> psio(new PSIO());
    if ( diis_iter_ == 0 ) {
       psio->open(PSIF_DCC_OVEC,PSIO_OPEN_NEW);
    }else {
       psio->open(PSIF_DCC_OVEC,PSIO_OPEN_OLD);
    }

    // Write the current solution vector.
    psio_address addr = PSIO_ZERO;
    psio->write(PSIF_DCC_OVEC,oldvector,(char*)vector1,dimdiis_*sizeof(double),addr,&addr);
    psio->write(PSIF_DCC_OVEC,oldvector,(char*)vector2,dimdiis_*sizeof(double),addr,&addr);
    psio->close(PSIF_DCC_OVEC,1);

    free(oldvector);
}

// Write current error vector to disk (in file PSIF_DCC_EVEC).
void DIIS::WriteErrorVector(double * vector1, double * vector2){

    // Name the entry in PSIF_DCC_EVEC according to the current
    // DIIS iteration.  If we already have maxdiis_ vectors, then
    // replace one.
    char * evector = (char*)malloc(1000*sizeof(char));
    if ( diis_iter_ <= maxdiis_ ){
       sprintf(evector,"evector%i",diis_iter_);
    }
    else{
       sprintf(evector,"evector%i",replace_diis_iter_);
    }

    std::shared_ptr<PSIO> psio(new PSIO());
    if ( diis_iter_ == 0 ) {
       // On the first iteration, write an entry to PSIF_DCC_EVEC
       // that will hold the error matrix.
       psio->open(PSIF_DCC_EVEC,PSIO_OPEN_NEW);
       double * temp = (double*)malloc(maxdiis_*maxdiis_*sizeof(double));
       memset((void*)temp,'\0',maxdiis_*maxdiis_*sizeof(double));
       psio->write_entry(PSIF_DCC_EVEC,"error matrix",(char*)&temp[0],maxdiis_*maxdiis_*sizeof(double));
       free(temp);
    }
    else {
       psio->open(PSIF_DCC_EVEC,PSIO_OPEN_OLD);
    }

    // Write current error vector.
    psio_address addr = PSIO_ZERO;
    psio->write(PSIF_DCC_EVEC,evector,(char*)vector1,dimdiis_*sizeof(double),addr,&addr);
    psio->write(PSIF_DCC_EVEC,evector,(char*)vector2,dimdiis_*sizeof(double),addr,&addr);
    psio->close(PSIF_DCC_EVEC,1);

    free(evector);
}

// Perform DIIS extrapolation.
void DIIS::Extrapolate(double * vector1, double * vector2){

    if ( diis_iter_ > 2 ) {

        // Compute coefficients for the extrapolation
        DIISCoefficients( diis_iter_ < maxdiis_ ? diis_iter_ : maxdiis_ );

        memset((void*)vector1,'\0',dimdiis_*sizeof(double));
        memset((void*)vector2,'\0',dimdiis_*sizeof(double));

        char * oldvector = (char*)malloc(1000*sizeof(char));
    
        std::shared_ptr<PSIO> psio(new PSIO());
        psio->open(PSIF_DCC_OVEC,PSIO_OPEN_OLD);
    
        int max = diis_iter_;
        if (max > maxdiis_) max = maxdiis_;
    
        // Read each of the old vectors from disk.
        for (int j = 1; j <= max; j++){
            sprintf(oldvector,"oldvector%i",j);
            psio_address addr = PSIO_ZERO;
            psio->read(PSIF_DCC_OVEC,oldvector,(char*)&tmp1_[0],dimdiis_*sizeof(double),addr,&addr);
            // Accumulate extrapolated vector.
            C_DAXPY(dimdiis_,diisvec_[j-1],tmp1_,1,vector1,1);

            psio->read(PSIF_DCC_OVEC,oldvector,(char*)&tmp1_[0],dimdiis_*sizeof(double),addr,&addr);
            // Accumulate extrapolated vector.
            C_DAXPY(dimdiis_,diisvec_[j-1],tmp1_,1,vector2,1);
        }
        psio->close(PSIF_DCC_OVEC,1);
        free(oldvector);
    }

    if (diis_iter_ <= maxdiis_){
        diis_iter_++;
    }
    else {
        // If we already have maxdiis_ vectors, choose the one with
        // the largest error as the one to replace.
        std::shared_ptr<PSIO> psio(new PSIO());
        psio->open(PSIF_DCC_EVEC,PSIO_OPEN_OLD);
        int jmax   = 1;
        double max = -1.0e99;
        char * evector   = (char*)malloc(1000*sizeof(char));
        for (int j = 1; j <= maxdiis_; j++){
            sprintf(evector,"evector%i",j);
            psio_address addr = PSIO_ZERO;
            psio->read(PSIF_DCC_EVEC,evector,(char*)tmp1_,dimdiis_*sizeof(double),addr,&addr);
            double nrm = C_DNRM2(dimdiis_,tmp1_,1);
            psio->read(PSIF_DCC_EVEC,evector,(char*)tmp1_,dimdiis_*sizeof(double),addr,&addr);
            nrm       += C_DNRM2(dimdiis_,tmp1_,1);
            nrm = 0.5 * nrm;
            if ( nrm > max ) {
                max  = nrm;
                jmax = j;
            }
        }
        psio->close(PSIF_DCC_EVEC,1);
        replace_diis_iter_ = jmax;
        free(evector);
    }
    //else if (replace_diis_iter_ < maxdiis_) replace_diis_iter_++;
    //else                                    replace_diis_iter_ = 1;
}

// Evaluate extrapolation coefficients for DIIS.
void DIIS::DIISCoefficients(int nvec){

    // Allocate memory for small matrices/vectors.
    int * ipiv    = (int*)malloc((nvec+1)*sizeof(int));
    double * temp = (double*)malloc(sizeof(double)*maxdiis_*maxdiis_);
    double * A    = (double*)malloc(sizeof(double)*(nvec+1)*(nvec+1));
    double * B    = (double*)malloc(sizeof(double)*(nvec+1));
    memset((void*)A,'\0',(nvec+1)*(nvec+1)*sizeof(double));
    memset((void*)B,'\0',(nvec+1)*sizeof(double));
    B[nvec] = -1.;

    char * evector = (char*)malloc(1000*sizeof(char));

    std::shared_ptr<PSIO> psio(new PSIO());
    psio->open(PSIF_DCC_EVEC,PSIO_OPEN_OLD);

    // Read in the previous error matrix, so we don't have 
    // to build the entire thing each iteration.
    psio->read_entry(PSIF_DCC_EVEC,"error matrix",(char*)&temp[0],maxdiis_*maxdiis_*sizeof(double));

    // Reshape the error matrix, in case its dimension is less than maxdiis_.
    for (int i = 0; i < nvec; i++){
        for (int j = 0; j < nvec; j++){
            A[i*(nvec+1)+j] = temp[i*maxdiis_+j];
        }
    }

    if (nvec <= 3) {
        // At early iterations, just build the whole matrix.

        
        for (int i = 0; i < nvec; i++) {
            sprintf(evector,"evector%i",i+1);
            psio->read_entry(PSIF_DCC_EVEC,evector,(char*)tmp1_,2*dimdiis_*sizeof(double));
            for (int j = i+1; j < nvec; j++){
                sprintf(evector,"evector%i",j+1);
                psio->read_entry(PSIF_DCC_EVEC,evector,(char*)tmp2_,2*dimdiis_*sizeof(double));
                double sum  = C_DDOT(2*dimdiis_,tmp1_,1,tmp2_,1);
                A[i*(nvec+1)+j] = sum;
                A[j*(nvec+1)+i] = sum;
            }
            double sum  = C_DDOT(2*dimdiis_,tmp1_,1,tmp1_,1);
            A[i*(nvec+1)+i] = sum;
        }

    }else {
        // At later iterations, don't build the whote matrix.
        // Just replace one row/column.

        // Which row/column will be replaced?
        int i = nvec < maxdiis_ ? nvec - 1 : replace_diis_iter_ - 1;

        sprintf(evector,"evector%i",i+1);
        psio->read_entry(PSIF_DCC_EVEC,evector,(char*)tmp1_,2*dimdiis_*sizeof(double));
        for (int j = 0; j < nvec; j++){
            sprintf(evector,"evector%i",j+1);
            psio->read_entry(PSIF_DCC_EVEC,evector,(char*)tmp2_,2*dimdiis_*sizeof(double));
            double sum  = C_DDOT(2*dimdiis_,tmp1_,1,tmp2_,1);
            A[i*(nvec+1)+j] = sum;
            A[j*(nvec+1)+i] = sum;
        }
    }

    int j = nvec;
    for (int i = 0; i < (nvec+1); i++){
        A[j*(nvec+1)+i] = -1.0;
        A[i*(nvec+1)+j] = -1.0;
    }
    A[(nvec+1)*(nvec+1)-1] = 0.0;

    // Write error matrix for next iteration.
    for (int i = 0; i < nvec; i++){
        for (int j = 0; j < nvec; j++){
            temp[i*maxdiis_+j] = A[i*(nvec+1)+j];
        }
    }
    psio->write_entry(PSIF_DCC_EVEC,"error matrix",(char*)&temp[0],maxdiis_*maxdiis_*sizeof(double));
    free(temp);
    psio->close(PSIF_DCC_EVEC,1);
    free(evector);

    // Solve the set of linear equations for the extrapolation coefficients
    int nrhs = 1;
    int lda  = nvec+1;
    int ldb  = nvec+1;
    C_DGESV(nvec+1,nrhs,A,lda,ipiv,B,ldb);
    C_DCOPY(nvec,B,1,diisvec_,1);

    //outfile->Printf("\n");
    //outfile->Printf("    ==> DIIS Expansion Coefficients <==\n");
    //outfile->Printf("\n");
    //for (int i = 0; i < nvec; i++) outfile->Printf("        c[%i] %20.12lf\n",i,diisvec_[i]);
    //outfile->Printf("\n");

    free(A);
    free(B);
    free(ipiv);
}

}} // end of namespaces
