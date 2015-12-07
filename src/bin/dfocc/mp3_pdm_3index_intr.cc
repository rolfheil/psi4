/*
 *@BEGIN LICENSE
 *
 * PSI4: an ab initio quantum chemistry software package
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
 *@END LICENSE
 */

#include <libqt/qt.h>
#include "defines.h"
#include "dfocc.h"

using namespace psi;
using namespace std;


namespace psi{ namespace dfoccwave{
  
void DFOCC::mp3_pdm_3index_intr()
{

    // defs
    SharedTensor2d K, L, T, T1, T2, U, Tau, V, V2, Vij, Vai, Vab, X, Y, Z;
    SharedTensor2d Vijka, Vijak;
    SharedTensor2d Vs, Va, Ts, Ta, S, A;
    SharedTensor2d Yt, Yp, TAA, TBB, TAB;

if (reference_ == "RESTRICTED") {
    // Read DF integrals
    bQijA = SharedTensor2d(new Tensor2d("DF_BASIS_CC B (Q|IJ)", nQ, naoccA, naoccA));
    bQiaA = SharedTensor2d(new Tensor2d("DF_BASIS_CC B (Q|IA)", nQ, naoccA, navirA));
    bQabA = SharedTensor2d(new Tensor2d("DF_BASIS_CC B (Q|AB)", nQ, navirA, navirA));
    bQijA->read(psio_, PSIF_DFOCC_INTS);
    bQiaA->read(psio_, PSIF_DFOCC_INTS);
    bQabA->read(psio_, PSIF_DFOCC_INTS, true, true);

    // Read amps
    U = SharedTensor2d(new Tensor2d("U2 (IA|JB)", naoccA, navirA, naoccA, navirA));
    U->read_symm(psio_, PSIF_DFOCC_AMPS);
    T1 = SharedTensor2d(new Tensor2d("T2_1 (IA|JB)", naoccA, navirA, naoccA, navirA));
    T1->read_symm(psio_, PSIF_DFOCC_AMPS);

    // T(Q,ia) = \sum_{jb} b_jb^Q U_ij^ab = \sum_{jb} b(Q,jb) U(jb,ia)
    T = SharedTensor2d(new Tensor2d("T2 (Q|IA)", nQ, naoccA, navirA));
    T->gemm(false, false, bQiaA, U, 1.0, 0.0);
    //T->cont343("IA", "JB", "IAJB", false, bQiaA, U, 1.0, 0.0); // it works
    T->write(psio_, PSIF_DFOCC_AMPS);
    T.reset();

    // Build G_mi and G_ae
    // G_mi = \sum_{n,e,f} U_mn^ef L_in^ef(1) = U(mn,ef) L(in,ef)(1)
    T = SharedTensor2d(new Tensor2d("U2 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    T->sort(1324, U, 1.0, 0.0);
    U.reset();
    L = SharedTensor2d(new Tensor2d("T2_1 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    L->sort(1324, T1, 1.0, 0.0);
    GijA->contract(false, true, naoccA, naoccA, naoccA*navirA*navirA, T, L, 1.0, 0.0);
    //GijA->cont442("MI", "MNEF", "INEF", false, false, T, L, 1.0, 0.0); // it works

    // G_ae = -\sum_{m,n,f} U_mn^ef L_mn^af(1) = L(mn,fa)(1) U(mn,fe) 
    GabA->contract(true, false, navirA, navirA, naoccA*naoccA*navirA, L, T, -1.0, 0.0);
    T.reset();
    L.reset();

    // Read amps
    T2 = SharedTensor2d(new Tensor2d("T2_2 (IA|JB)", naoccA, navirA, naoccA, navirA));
    T2->read_symm(psio_, PSIF_DFOCC_AMPS);

    // G_mi += \sum_{n,e,f} U_mn^ef(1) L_in^ef(2) = U(mn,ef)(1) L(in,ef)(2) : MP3
    U = SharedTensor2d(new Tensor2d("U2_1 (IA|JB)", naoccA, navirA, naoccA, navirA));
    ccsd_u2_amps(U,T1);
    T = SharedTensor2d(new Tensor2d("U2_1 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    T->sort(1324, U, 1.0, 0.0);
    U.reset();
    L = SharedTensor2d(new Tensor2d("T2_2 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    L->sort(1324, T2, 1.0, 0.0);
    T2.reset();
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GijA->contract(false, true, naoccA, naoccA, naoccA*navirA*navirA, T, L, 1.0, 1.0);
    }
    // G_mi += 1/2 \sum_{n,e,f} U_mn^ef(1) L_in^ef(2) = U(mn,ef)(1) L(in,ef)(2) : MP2.5
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GijA->contract(false, true, naoccA, naoccA, naoccA*navirA*navirA, T, L, 0.5, 1.0);
    }

    // G_ae += -\sum_{m,n,f} U_mn^ef(1) L_mn^af(2) = L(mn,fa)(2) U(mn,fe)(1) : MP3 
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GabA->contract(true, false, navirA, navirA, naoccA*naoccA*navirA, L, T, -1.0, 1.0);
    }
    // G_ae += -1/2 \sum_{m,n,f} U_mn^ef(1) L_mn^af(2) = L(mn,fa)(2) U(mn,fe)(1) : MP2.5 
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GabA->contract(true, false, navirA, navirA, naoccA*naoccA*navirA, L, T, -0.5, 1.0);
    }
    T.reset();
    L.reset();


    // Build V_ijkl
    // V_ijkl = \sum_{ef} T_ij^ef(1) L_kl^ef(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    U->sort(1324, T1, 1.0, 0.0);
    V = SharedTensor2d(new Tensor2d("V <IJ|KL>", naoccA, naoccA, naoccA, naoccA));
    V->gemm(false, true, U, U, 1.0, 0.0);
    U.reset();

    // V_ij^Q = \sum_{mn} (2*V_imjn - V_imnj) b_mn^Q = B(Q,mn) Y(mn,ij)
    X = SharedTensor2d(new Tensor2d("X <IJ|KL>", naoccA, naoccA, naoccA, naoccA));
    // X_imjn = 2*V_imjn - V_imnj
    X->tei_cs1_anti_symm(V,V);
    V.reset();
    // Y_mnij = X_imjn
    Y = SharedTensor2d(new Tensor2d("Y <IJ|KL>", naoccA, naoccA, naoccA, naoccA));
    Y->sort(2413, X, 1.0, 0.0);
    X.reset();
    T = SharedTensor2d(new Tensor2d("V (Q|IJ)", nQ, naoccA, naoccA));
    T->gemm(false, false, bQijA, Y, 1.0, 0.0); 
    Y.reset();
    T->write(psio_, PSIF_DFOCC_AMPS);
    T.reset();

    // Build V_iajb
    // V_iajb = 1/2 \sum_{me} T'(ib,me)(1) L'(me,ja)(1)
    T = SharedTensor2d(new Tensor2d("T2p (IB|JA)", naoccA, navirA, naoccA, navirA));
    ccsd_t2_prime_amps(T,T1);
    X = SharedTensor2d(new Tensor2d("X (IB|JA)", naoccA, navirA, naoccA, navirA));
    X->gemm(false, false, T, T, 0.5, 0.0);
    T.reset();
    V = SharedTensor2d(new Tensor2d("V (IA|JB)", naoccA, navirA, naoccA, navirA));
    V->sort(1432, X, 1.0, 0.0);
    X.reset();
    //V->write(psio_, PSIF_DFOCC_AMPS);

    // V_ij^Q' <= 2\sum_{ef} V_iejf b_ef^Q
    Vij = SharedTensor2d(new Tensor2d("Vp (Q|IJ)", nQ, naoccA, naoccA));
    X = SharedTensor2d(new Tensor2d("X (AB|IJ)", navirA, navirA, naoccA, naoccA));
    X->sort(2413, V, 1.0, 0.0);
    Vij->gemm(false, false, bQabA, X, 2.0, 0.0); 
    X.reset();

    // V_ab^Q = 2\sum_{mn} V_manb b_mn^Q
    X = SharedTensor2d(new Tensor2d("X (MN|AB)", naoccA, naoccA, navirA, navirA));
    X->sort(1324, V, 1.0, 0.0);
    //V.reset();
    Vab = SharedTensor2d(new Tensor2d("V (Q|AB)", nQ, navirA, navirA));
    Vab->gemm(false, false, bQijA, X, 2.0, 0.0); 
    X.reset();
    Vab->write(psio_, PSIF_DFOCC_AMPS);
    Vab.reset();

    // Build y_ia^Q : Part 1
    // Y(me,ia) = V(ie,ma)
    Y = SharedTensor2d(new Tensor2d("Y (ME|IA)", naoccA, navirA, naoccA, navirA));
    Y->sort(3214, V, 1.0, 0.0);
    V.reset();
    // y_ia^Q <= 2\sum(me) b(Q,me) * Y(me,ia)
    Z = SharedTensor2d(new Tensor2d("Y (Q|IA)", nQ, naoccA, navirA));
    Z->gemm(false, false, bQiaA, Y, 2.0, 0.0); 
    Y.reset();
    Z->write(psio_, PSIF_DFOCC_AMPS);
    Z.reset();

    // Build V_iabj
    // V_iabj = -1/2 \sum_{me} U(ib,me)(1) L(me,ja)(1)
    U = SharedTensor2d(new Tensor2d("U2_1 (IA|JB)", naoccA, navirA, naoccA, navirA));
    ccsd_u2_amps(U,T1);
    X = SharedTensor2d(new Tensor2d("X (IB|JA)", naoccA, navirA, naoccA, navirA));
    X->gemm(false, false, U, T1, -0.5, 0.0);
    U.reset();
    // V_iabj += 1/2 \sum_{me} T(ib,me)(1) L'(me,ja)(1)
    L = SharedTensor2d(new Tensor2d("L2p (IB|JA)", naoccA, navirA, naoccA, navirA));
    ccsd_t2_prime_amps(L,T1);
    X->gemm(false, false, T1, L, 0.5, 1.0);
    T1.reset();
    L.reset();
    V = SharedTensor2d(new Tensor2d("V (IA|BJ)", naoccA, navirA, navirA, naoccA));
    V->sort(1423, X, 1.0, 0.0);
    X.reset();
    //V->write(psio_, PSIF_DFOCC_AMPS);

    // V_ij^Q' -= \sum_{ef} V_iefj b_ef^Q
    X = SharedTensor2d(new Tensor2d("X (AB|IJ)", navirA, navirA, naoccA, naoccA));
    X->sort(2314, V, 1.0, 0.0);
    Vij->gemm(false, false, bQabA, X, -1.0, 1.0); 
    X.reset();
    Vij->write(psio_, PSIF_DFOCC_AMPS);
    Vij.reset();

    // V_ab^Q -= \sum_{mn} V_mabn b_mn^Q
    X = SharedTensor2d(new Tensor2d("X (MN|AB)", naoccA, naoccA, navirA, navirA));
    X->sort(1423, V, 1.0, 0.0);
    //V.reset();
    Vab = SharedTensor2d(new Tensor2d("V (Q|AB)", nQ, navirA, navirA));
    Vab->read(psio_, PSIF_DFOCC_AMPS);
    Vab->gemm(false, false, bQijA, X, -1.0, 1.0); 
    X.reset();
    Vab->write(psio_, PSIF_DFOCC_AMPS);
    Vab.reset();

    // Build y_ia^Q : Part 2
    // Y(me,ia) = V(ie,am)
    Y = SharedTensor2d(new Tensor2d("Y (ME|IA)", naoccA, navirA, naoccA, navirA));
    Y->sort(4213, V, 1.0, 0.0);
    V.reset();
    // y_ia^Q <= -4\sum(me) b(Q,me) * Y(me,ia)
    Z = SharedTensor2d(new Tensor2d("Y (Q|IA)", nQ, naoccA, navirA));
    Z->read(psio_, PSIF_DFOCC_AMPS);
    Z->gemm(false, false, bQiaA, Y, -4.0, 1.0); 
    Y.reset();
    Z->write(psio_, PSIF_DFOCC_AMPS);
    Z.reset();

    /*
    // Build Yt_ijab
    // Yt_ijab -= V_jabi + V_ibaj
    Yt = SharedTensor2d(new Tensor2d("Y2 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    V = SharedTensor2d(new Tensor2d("V (IA|BJ)", naoccA, navirA, navirA, naoccA));
    V->read(psio_, PSIF_DFOCC_AMPS);
    Yt->sort(4123, V, -1.0, 0.0);
    Yt->sort(1432, V, -1.0, 1.0);
    V.reset();

    // Build Y'_ijab
    // Y'_ijab -= V_iajb + V_jbia
    Yp = SharedTensor2d(new Tensor2d("Y2p <IJ|AB>", naoccA, naoccA, navirA, navirA));
    V = SharedTensor2d(new Tensor2d("V (IA|JB)", naoccA, navirA, naoccA, navirA));
    V->read(psio_, PSIF_DFOCC_AMPS);
    Yp->sort(1324, V, -1.0, 0.0);
    Yp->sort(3142, V, -1.0, 1.0);
    V.reset();

    // Build y_ia^Q
    // X_imae = 2*Yt_imae - Yp_imea
    X = SharedTensor2d(new Tensor2d("X <IM|AE>", naoccA, naoccA, navirA, navirA));
    X->tei_cs1_anti_symm(Yt,Yp);
    Yt.reset();
    Yp.reset();
    // Y(me,ia) = X(im,ae)
    Y = SharedTensor2d(new Tensor2d("Y (ME|IA)", naoccA, navirA, naoccA, navirA));
    Y->sort(2413, X, 1.0, 0.0);
    X.reset();
    // y_ia^Q = \sum(me) b(Q,me) * Y(me,ia)
    Z = SharedTensor2d(new Tensor2d("Y (Q|IA)", nQ, naoccA, navirA));
    Z->gemm(false, false, bQiaA, Y, 1.0, 0.0); 
    Y.reset();
    Z->write(psio_, PSIF_DFOCC_AMPS);
    Z.reset();
    */

    // Free ints
    bQijA.reset();
    bQiaA.reset();
    bQabA.reset();
}// end if (reference_ == "RESTRICTED")

else if (reference_ == "UNRESTRICTED") {

    //=========================
    // G Intermediates
    //=========================

    // Build G_MI and G_AE
    // G_MI <= 1/2 \sum_{N,E,F} T(MN,EF) L(IN,EF)(1)
    T = SharedTensor2d(new Tensor2d("T2 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    T->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    L = SharedTensor2d(new Tensor2d("T2_1 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    L->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    GijA->contract(false, true, naoccA, naoccA, naoccA*navirA*navirA, T, L, 0.5, 0.0);

    // G_AE <= -1/2 \sum_{MNF} L(MN,FA)(1) T(MN,FE) 
    GabA->contract(true, false, navirA, navirA, naoccA*naoccA*navirA, L, T, -0.5, 0.0);
    T.reset();
    //L.reset();
    
    // G_MI += 1/2 \sum_{N,E,F} T(MN,EF)(1) L(IN,EF)(2) : MP3
    T = SharedTensor2d(new Tensor2d("T2_2 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    T->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GijA->contract(false, true, naoccA, naoccA, naoccA*navirA*navirA, L, T, 0.5, 1.0);
    }
    // G_MI += 1/4 \sum_{N,E,F} T(MN,EF)(1) L(IN,EF)(2) : MP2.5
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GijA->contract(false, true, naoccA, naoccA, naoccA*navirA*navirA, L, T, 0.25, 1.0);
    }

    // G_AE += -1/2 \sum_{MNF} L(MN,FA)(2) T(MN,FE)(1) : MP3
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GabA->contract(true, false, navirA, navirA, naoccA*naoccA*navirA, T, L, -0.5, 1.0);
    }
    // G_AE += -1/4 \sum_{MNF} L(MN,FA)(2) T(MN,FE)(1) : MP2.5
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GabA->contract(true, false, navirA, navirA, naoccA*naoccA*navirA, T, L, -0.25, 1.0);
    }
    T.reset();
    L.reset();

    // G_MI += \sum_{n,E,f} T(Mn,Ef) L(In,Ef)(1)
    T = SharedTensor2d(new Tensor2d("T2 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    T->read(psio_, PSIF_DFOCC_AMPS);
    L = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    L->read(psio_, PSIF_DFOCC_AMPS);
    GijA->contract(false, true, naoccA, naoccA, naoccB*navirA*navirB, T, L, 1.0, 1.0);

    // G_AE += - \sum_{Mnf} L(Mn,Af)(1) T(Mn,Ef)
    X = SharedTensor2d(new Tensor2d("T2_1 <Ij|bA>", naoccA, naoccB, navirB, navirA));
    X->sort(1243, L, 1.0, 0.0);
    L.reset();
    Y = SharedTensor2d(new Tensor2d("T2 <Ij|bA>", naoccA, naoccB, navirB, navirA));
    Y->sort(1243, T, 1.0, 0.0);
    T.reset();
    GabA->contract(true, false, navirA, navirA, naoccA*naoccB*navirB, X, Y, -1.0, 1.0);
    X.reset();
    Y.reset();

    // G_MI += \sum_{n,E,f} T(Mn,Ef)(1) L(In,Ef)(2) : MP3
    T = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    T->read(psio_, PSIF_DFOCC_AMPS);
    L = SharedTensor2d(new Tensor2d("T2_2 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    L->read(psio_, PSIF_DFOCC_AMPS);
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GijA->contract(false, true, naoccA, naoccA, naoccB*navirA*navirB, T, L, 1.0, 1.0);
    }
    // G_MI += 1/2 \sum_{n,E,f} T(Mn,Ef)(1) L(In,Ef)(2) : MP2.5
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GijA->contract(false, true, naoccA, naoccA, naoccB*navirA*navirB, T, L, 0.5, 1.0);
    }

    // G_AE += - \sum_{Mnf} L(Mn,Af)(2) T(Mn,Ef)(1) : MP3
    X = SharedTensor2d(new Tensor2d("T2_2 <Ij|bA>", naoccA, naoccB, navirB, navirA));
    X->sort(1243, L, 1.0, 0.0);
    L.reset();
    Y = SharedTensor2d(new Tensor2d("T2_1 <Ij|bA>", naoccA, naoccB, navirB, navirA));
    Y->sort(1243, T, 1.0, 0.0);
    T.reset();
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GabA->contract(true, false, navirA, navirA, naoccA*naoccB*navirB, X, Y, -1.0, 1.0);
    }
    // G_AE += -1/2 \sum_{Mnf} L(Mn,Af)(2) T(Mn,Ef)(1) : MP2.5
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GabA->contract(true, false, navirA, navirA, naoccA*naoccB*navirB, X, Y, -0.5, 1.0);
    }
    X.reset();
    Y.reset();

    // Build G_mi and G_ae
    // G_mi <= 1/2 \sum_{nef} T(mn,ef) L(in,ef)(1)
    T = SharedTensor2d(new Tensor2d("T2 <ij|ab>", naoccB, naoccB, navirB, navirB));
    T->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    L = SharedTensor2d(new Tensor2d("T2_1 <ij|ab>", naoccB, naoccB, navirB, navirB));
    L->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    GijB->contract(false, true, naoccB, naoccB, naoccB*navirB*navirB, T, L, 0.5, 0.0);

    // G_ae <= -1/2 \sum_mnf} L(mn,fa)(1) T(mn,fe) 
    GabB->contract(true, false, navirB, navirB, naoccB*naoccB*navirB, L, T, -0.5, 0.0);
    T.reset();
    //L.reset();
    
    // G_mi += 1/2 \sum_{nef} T(mn,ef)(1) L(in,ef)(2) : MP3
    T = SharedTensor2d(new Tensor2d("T2_2 <ij|ab>", naoccB, naoccB, navirB, navirB));
    T->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GijB->contract(false, true, naoccB, naoccB, naoccB*navirB*navirB, L, T, 0.5, 1.0);
    }
    // G_mi += 1/4 \sum_{nef} T(mn,ef)(1) L(in,ef)(2) : MP2.5
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GijB->contract(false, true, naoccB, naoccB, naoccB*navirB*navirB, L, T, 0.25, 1.0);
    }

    // G_ae += -1/2 \sum_{mnf} L(mn,fa)(2) T(mn,fe)(1) : MP3
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GabB->contract(true, false, navirB, navirB, naoccB*naoccB*navirB, T, L, -0.5, 1.0);
    }
    // G_ae += -1/4 \sum_{mnf} L(mn,fa)(2) T(mn,fe)(1) : MP2.5
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GabB->contract(true, false, navirB, navirB, naoccB*naoccB*navirB, T, L, -0.25, 1.0);
    }
    T.reset();
    L.reset();

    // G_ae += - \sum_{mNF} L(Nm,Fa)(1) T(Nm,Fe)
    T = SharedTensor2d(new Tensor2d("T2 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    T->read(psio_, PSIF_DFOCC_AMPS);
    L = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    L->read(psio_, PSIF_DFOCC_AMPS);
    GabB->contract(true, false, navirB, navirB, naoccB*naoccA*navirA, L, T, -1.0, 1.0);

    // G_mi += \sum_{N,e,F} T(Nm,Fe) L(Ni,Fe)(1)
    X = SharedTensor2d(new Tensor2d("T2 <jI|Ab>", naoccB, naoccA, navirA, navirB));
    X->sort(2134, T, 1.0, 0.0);
    T.reset();
    Y = SharedTensor2d(new Tensor2d("T2_1 <jI|Ab>", naoccB, naoccA, navirA, navirB));
    Y->sort(2134, L, 1.0, 0.0);
    L.reset();
    GijB->contract(false, true, naoccB, naoccB, naoccA*navirB*navirA, X, Y, 1.0, 1.0);
    X.reset();
    Y.reset();

    // G_ae += - \sum_{mNF} L(Nm,Fa)(2) T(Nm,Fe)(1) : MP3
    T = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    T->read(psio_, PSIF_DFOCC_AMPS);
    L = SharedTensor2d(new Tensor2d("T2_2 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    L->read(psio_, PSIF_DFOCC_AMPS);
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GabB->contract(true, false, navirB, navirB, naoccB*naoccA*navirA, L, T, -1.0, 1.0);
    }
    // G_ae += -1/2 \sum_{mNF} L(Nm,Fa)(2) T(Nm,Fe)(1) : MP2.5
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GabB->contract(true, false, navirB, navirB, naoccB*naoccA*navirA, L, T, -0.5, 1.0);
    }

    // G_mi += \sum_{N,e,F} T(Nm,Fe)(1) L(Ni,Fe)(2) : MP3
    X = SharedTensor2d(new Tensor2d("T2_1 <jI|Ab>", naoccB, naoccA, navirA, navirB));
    X->sort(2134, T, 1.0, 0.0);
    T.reset();
    Y = SharedTensor2d(new Tensor2d("T2_2 <jI|Ab>", naoccB, naoccA, navirA, navirB));
    Y->sort(2134, L, 1.0, 0.0);
    L.reset();
    if (wfn_type_ == "DF-OMP3" || wfn_type_ == "CD-OMP3") {
        GijB->contract(false, true, naoccB, naoccB, naoccA*navirB*navirA, X, Y, 1.0, 1.0);
    }
    // G_mi += 1/2 \sum_{N,e,F} T(Nm,Fe)(1) L(Ni,Fe)(2) : MP2.5
    else if (wfn_type_ == "DF-OMP2.5" || wfn_type_ == "CD-OMP2.5") {
        GijB->contract(false, true, naoccB, naoccB, naoccA*navirB*navirA, X, Y, 0.5, 1.0);
    }
    X.reset();
    Y.reset();

    //outfile->Printf("\tG intermediates are done.\n");

    //=========================
    // 3-Index Intermediates
    //=========================

    // Read DF integrals
    bQijA = SharedTensor2d(new Tensor2d("DF_BASIS_CC B (Q|IJ)", nQ, naoccA, naoccA));
    bQijB = SharedTensor2d(new Tensor2d("DF_BASIS_CC B (Q|ij)", nQ, naoccB, naoccB));
    bQiaA = SharedTensor2d(new Tensor2d("DF_BASIS_CC B (Q|IA)", nQ, naoccA, navirA));
    bQiaB = SharedTensor2d(new Tensor2d("DF_BASIS_CC B (Q|ia)", nQ, naoccB, navirB));
    bQabA = SharedTensor2d(new Tensor2d("DF_BASIS_CC B (Q|AB)", nQ, navirA, navirA));
    bQabB = SharedTensor2d(new Tensor2d("DF_BASIS_CC B (Q|ab)", nQ, navirB, navirB));
    bQijA->read(psio_, PSIF_DFOCC_INTS);
    bQijB->read(psio_, PSIF_DFOCC_INTS);
    bQiaA->read(psio_, PSIF_DFOCC_INTS);
    bQiaB->read(psio_, PSIF_DFOCC_INTS);
    bQabA->read(psio_, PSIF_DFOCC_INTS, true, true);
    bQabB->read(psio_, PSIF_DFOCC_INTS, true, true);

    // T(Q,IA) <= \sum_{JB} b(Q,JB) T(JB,IA) 
    TAA = SharedTensor2d(new Tensor2d("T2 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    TAA->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    U = SharedTensor2d(new Tensor2d("T2 (IA|JB)", naoccA, navirA, naoccA, navirA));
    U->sort(1324, TAA, 1.0, 0.0);
    TAA.reset();
    T = SharedTensor2d(new Tensor2d("T2 (Q|IA)", nQ, naoccA, navirA));
    T->gemm(false, false, bQiaA, U, 1.0, 0.0);
    U.reset();
    // T(Q,IA) += \sum_{jb} b(Q,jb) T(IA,jb)
    TAB = SharedTensor2d(new Tensor2d("T2 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    TAB->read(psio_, PSIF_DFOCC_AMPS);
    U = SharedTensor2d(new Tensor2d("T2 (IA|jb)", naoccA, navirA, naoccB, navirB));
    U->sort(1324, TAB, 1.0, 0.0);
    TAB.reset();
    T->gemm(false, true, bQiaB, U, 1.0, 1.0);
    T->write(psio_, PSIF_DFOCC_AMPS);
    T.reset();

    // T(Q,ia) <= \sum_{JB} b(Q,JB) T(JB,ia)
    T = SharedTensor2d(new Tensor2d("T2 (Q|ia)", nQ, naoccB, navirB));
    T->gemm(false, false, bQiaA, U, 1.0, 0.0);
    U.reset();
    // T(Q,ia) <= \sum_{jb} b(Q,jb) T(jb,ia)
    TBB = SharedTensor2d(new Tensor2d("T2 <ij|ab>", naoccB, naoccB, navirB, navirB));
    TBB->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    U = SharedTensor2d(new Tensor2d("T2 (ia|jb)", naoccB, navirB, naoccB, navirB));
    U->sort(1324, TBB, 1.0, 0.0);
    TBB.reset();
    T->gemm(false, false, bQiaB, U, 1.0, 1.0);
    U.reset();
    T->write(psio_, PSIF_DFOCC_AMPS);
    T.reset();

    //=========================
    // V_ijkl & V_ij^Q
    //=========================

    // V_IJKL = 1/2 \sum_{EF} T_IJ^EF(1) L_KL^EF(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    U->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    V = SharedTensor2d(new Tensor2d("V <IJ|KL>", naoccA, naoccA, naoccA, naoccA));
    V->gemm(false, true, U, U, 0.5, 0.0);
    U.reset();

    // V_IJ^Q <= \sum_{MN} V_IMJN b_MN^Q = B(Q,MN) Y(MN,IJ)
    // Y_MNIJ = V_IMJN
    Y = SharedTensor2d(new Tensor2d("Y <IJ|KL>", naoccA, naoccA, naoccA, naoccA));
    Y->sort(2413, V, 1.0, 0.0);
    V.reset();
    T = SharedTensor2d(new Tensor2d("V (Q|IJ)", nQ, naoccA, naoccA));
    T->gemm(false, false, bQijA, Y, 1.0, 0.0); 
    Y.reset();
    T->write(psio_, PSIF_DFOCC_AMPS);
    T.reset();

    // V_ijkl = 1/2 \sum_{ef} T_ij^ef(1) L_kl^ef(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <ij|ab>", naoccB, naoccB, navirB, navirB));
    U->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    V = SharedTensor2d(new Tensor2d("V <ij|kl>", naoccB, naoccB, naoccB, naoccB));
    V->gemm(false, true, U, U, 0.5, 0.0);
    U.reset();

    // V_ij^Q <= \sum_{mn} V_imjn b_mn^Q = B(Q,mn) Y(mn,ij)
    // Y_mnij = V_imjn
    Y = SharedTensor2d(new Tensor2d("Y <ij|kl>", naoccB, naoccB, naoccB, naoccB));
    Y->sort(2413, V, 1.0, 0.0);
    V.reset();
    T = SharedTensor2d(new Tensor2d("V (Q|ij)", nQ, naoccB, naoccB));
    T->gemm(false, false, bQijB, Y, 1.0, 0.0); 
    Y.reset();
    T->write(psio_, PSIF_DFOCC_AMPS);
    T.reset();

    // V_IjKl = \sum_{Ef} T_Ij^Ef(1) L_Kl^Ef(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    U->read(psio_, PSIF_DFOCC_AMPS);
    V = SharedTensor2d(new Tensor2d("V <Ij|Kl>", naoccA, naoccB, naoccA, naoccB));
    V->gemm(false, true, U, U, 1.0, 0.0);
    U.reset();

    // V_IJ^Q += \sum_{mn} V_ImJn b_mn^Q = B(Q,mn) Y(mn,IJ)
    // Y_mnIJ = V_ImJn
    Y = SharedTensor2d(new Tensor2d("Y <mn|IJ>", naoccB, naoccB, naoccA, naoccA));
    Y->sort(2413, V, 1.0, 0.0);
    T = SharedTensor2d(new Tensor2d("V (Q|IJ)", nQ, naoccA, naoccA));
    T->read(psio_, PSIF_DFOCC_AMPS);
    T->gemm(false, false, bQijB, Y, 1.0, 1.0); 
    Y.reset();
    T->write(psio_, PSIF_DFOCC_AMPS);
    T.reset();

    // V_ij^Q += \sum_{MN} V_MiNj b_MN^Q = B(Q,MN) Y(MN,ij)
    // Y_MNij = V_MiNj
    Y = SharedTensor2d(new Tensor2d("Y <MN|ij>", naoccA, naoccA, naoccB, naoccB));
    Y->sort(1324, V, 1.0, 0.0);
    V.reset();
    T = SharedTensor2d(new Tensor2d("V (Q|ij)", nQ, naoccB, naoccB));
    T->read(psio_, PSIF_DFOCC_AMPS);
    T->gemm(false, false, bQijA, Y, 1.0, 1.0); 
    Y.reset();
    T->write(psio_, PSIF_DFOCC_AMPS);
    T.reset();

    //=========================
    // V_IAJB
    //=========================

    // V_IAJB <= 1/2 \sum_{ME} T(IB,ME)(1) L(ME,JA)(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    U->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    T = SharedTensor2d(new Tensor2d("T2_1 (IB|JA)", naoccA, navirA, naoccA, navirA));
    T->sort(1324, U, 1.0, 0.0);
    U.reset();
    X = SharedTensor2d(new Tensor2d("X (IB|JA)", naoccA, navirA, naoccA, navirA));
    X->gemm(false, false, T, T, 0.5, 0.0);
    T.reset();
    // V_IAJB += 1/2 \sum_{me} T(IB,me)(1) L(JA,me)(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    U->read(psio_, PSIF_DFOCC_AMPS);
    T = SharedTensor2d(new Tensor2d("T2_1 (IA|jb)", naoccA, navirA, naoccB, navirB));
    T->sort(1324, U, 1.0, 0.0);
    U.reset();
    X->gemm(false, true, T, T, 0.5, 1.0);
    T.reset();
    V = SharedTensor2d(new Tensor2d("V (IA|JB)", naoccA, navirA, naoccA, navirA));
    V->sort(1432, X, 1.0, 0.0);
    X.reset();
    //V->write(psio_, PSIF_DFOCC_AMPS);

    // V_IJ^Q' <= \sum_{EF} V_IEJF b_EF^Q
    Vij = SharedTensor2d(new Tensor2d("Vp (Q|IJ)", nQ, naoccA, naoccA));
    X = SharedTensor2d(new Tensor2d("X (AB|IJ)", navirA, navirA, naoccA, naoccA));
    X->sort(2413, V, 1.0, 0.0);
    Vij->gemm(false, false, bQabA, X, 1.0, 0.0); 
    X.reset();
    Vij->write(psio_, PSIF_DFOCC_AMPS);
    Vij.reset();

    // y_IA^Q <= 2 \sum(ME) V_IEMA b_ME^Q
    // Y(ME,IA) = V(IE,MA)
    Y = SharedTensor2d(new Tensor2d("Y (ME|IA)", naoccA, navirA, naoccA, navirA));
    Y->sort(3214, V, 1.0, 0.0);
    Z = SharedTensor2d(new Tensor2d("Y (Q|IA)", nQ, naoccA, navirA));
    Z->gemm(false, false, bQiaA, Y, 2.0, 0.0); 
    Y.reset();
    Z->write(psio_, PSIF_DFOCC_AMPS);
    Z.reset();

    // V_AB^Q <= \sum_{MN} V_MANB b_MN^Q
    X = SharedTensor2d(new Tensor2d("X (MN|AB)", naoccA, naoccA, navirA, navirA));
    X->sort(1324, V, 1.0, 0.0);
    V.reset();
    Vab = SharedTensor2d(new Tensor2d("V (Q|AB)", nQ, navirA, navirA));
    Vab->gemm(false, false, bQijA, X, 1.0, 0.0); 
    X.reset();
    Vab->write(psio_, PSIF_DFOCC_AMPS);
    Vab.reset();

    //=========================
    // V_iajb
    //=========================

    // V_iajb <= 1/2 \sum_{me} T(ib,me)(1) L(me,ja)(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <ij|ab>", naoccB, naoccB, navirB, navirB));
    U->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    T = SharedTensor2d(new Tensor2d("T2_1 (ib|ja)", naoccB, navirB, naoccB, navirB));
    T->sort(1324, U, 1.0, 0.0);
    U.reset();
    X = SharedTensor2d(new Tensor2d("X (ib|ja)", naoccB, navirB, naoccB, navirB));
    X->gemm(false, false, T, T, 0.5, 0.0);
    T.reset();
    // V_iajb += 1/2 \sum_{ME} T(me,IB)(1) L(me,JA)(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    U->read(psio_, PSIF_DFOCC_AMPS);
    T = SharedTensor2d(new Tensor2d("T2_1 (IA|jb)", naoccA, navirA, naoccB, navirB));
    T->sort(1324, U, 1.0, 0.0);
    U.reset();
    X->gemm(true, false, T, T, 0.5, 1.0);
    T.reset();
    V = SharedTensor2d(new Tensor2d("V (ia|jb)", naoccB, navirB, naoccB, navirB));
    V->sort(1432, X, 1.0, 0.0);
    X.reset();
    //V->write(psio_, PSIF_DFOCC_AMPS);

    // V_ij^Q' <= \sum_{ef} V_iejf b_ef^Q
    Vij = SharedTensor2d(new Tensor2d("Vp (Q|ij)", nQ, naoccB, naoccB));
    X = SharedTensor2d(new Tensor2d("X (ab|ij)", navirB, navirB, naoccB, naoccB));
    X->sort(2413, V, 1.0, 0.0);
    Vij->gemm(false, false, bQabB, X, 1.0, 0.0);
    X.reset();
    Vij->write(psio_, PSIF_DFOCC_AMPS);
    Vij.reset();

    // y_ia^Q <= 2 \sum(me) V_iema b_me^Q
    // Y(me,ia) = V(ie,ma)
    Y = SharedTensor2d(new Tensor2d("Y (me|ia)", naoccB, navirB, naoccB, navirB));
    Y->sort(3214, V, 1.0, 0.0);
    Z = SharedTensor2d(new Tensor2d("Y (Q|ia)", nQ, naoccB, navirB));
    Z->gemm(false, false, bQiaB, Y, 2.0, 0.0); 
    Y.reset();
    Z->write(psio_, PSIF_DFOCC_AMPS);
    Z.reset();

    // V_ab^Q <= \sum_{mn} V_manb b_mn^Q
    X = SharedTensor2d(new Tensor2d("X (mn|ab)", naoccB, naoccB, navirB, navirB));
    X->sort(1324, V, 1.0, 0.0);
    V.reset();
    Vab = SharedTensor2d(new Tensor2d("V (Q|ab)", nQ, navirB, navirB));
    Vab->gemm(false, false, bQijB, X, 1.0, 0.0);
    X.reset();
    Vab->write(psio_, PSIF_DFOCC_AMPS);
    Vab.reset();

    //=========================
    // V_IaJb
    //=========================

    // V_IaJb += 1/2 \sum_{mE} T'(Ib,mE)(1) L'(Ja,mE)(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    U->read(psio_, PSIF_DFOCC_AMPS);
    T = SharedTensor2d(new Tensor2d("T2p_1 (Ib|jA)", naoccA, navirB, naoccB, navirA));
    T->sort(1423, U, 1.0, 0.0);
    U.reset();
    X = SharedTensor2d(new Tensor2d("X (Ib|Ja)", naoccA, navirB, naoccA, navirB));
    X->gemm(false, true, T, T, 0.5, 0.0);
    T.reset();
    V = SharedTensor2d(new Tensor2d("V (Ia|Jb)", naoccA, navirB, naoccA, navirB));
    V->sort(1432, X, 1.0, 0.0);
    X.reset();
    //V->write(psio_, PSIF_DFOCC_AMPS);

    // V_IJ^Q' += \sum_{ef} V_IeJf b_ef^Q
    Vij = SharedTensor2d(new Tensor2d("Vp (Q|IJ)", nQ, naoccA, naoccA));
    Vij->read(psio_, PSIF_DFOCC_AMPS);
    X = SharedTensor2d(new Tensor2d("X (ef|IJ)", navirB, navirB, naoccA, naoccA));
    X->sort(2413, V, 1.0, 0.0);
    Vij->gemm(false, false, bQabB, X, 1.0, 1.0); 
    X.reset();
    Vij->write(psio_, PSIF_DFOCC_AMPS);
    Vij.reset();

    // V_ab^Q += \sum_{MN} V_MaNb b_MN^Q
    X = SharedTensor2d(new Tensor2d("X (MN|ab)", naoccA, naoccA, navirB, navirB));
    X->sort(1324, V, 1.0, 0.0);
    V.reset();
    Vab = SharedTensor2d(new Tensor2d("V (Q|ab)", nQ, navirB, navirB));
    Vab->read(psio_, PSIF_DFOCC_AMPS);
    Vab->gemm(false, false, bQijA, X, 1.0, 1.0);
    X.reset();
    Vab->write(psio_, PSIF_DFOCC_AMPS);
    Vab.reset();

    //=========================
    // V_iAjB
    //=========================

    // V_iAjB += 1/2 \sum_{Me} T'(Me,iB)(1) L'(Me,jA)(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    U->read(psio_, PSIF_DFOCC_AMPS);
    T = SharedTensor2d(new Tensor2d("T2p_1 (Ib|jA)", naoccA, navirB, naoccB, navirA));
    T->sort(1423, U, 1.0, 0.0);
    U.reset();
    X = SharedTensor2d(new Tensor2d("X (iB|jA)", naoccB, navirA, naoccB, navirA));
    X->gemm(true, false, T, T, 0.5, 0.0);
    T.reset();
    V = SharedTensor2d(new Tensor2d("V (iA|jB)", naoccB, navirA, naoccB, navirA));
    V->sort(1432, X, 1.0, 0.0);
    X.reset();
    //V->write(psio_, PSIF_DFOCC_AMPS);

    // V_ij^Q' += \sum_{EF} V_iEjF b_EF^Q
    Vij = SharedTensor2d(new Tensor2d("Vp (Q|ij)", nQ, naoccB, naoccB));
    Vij->read(psio_, PSIF_DFOCC_AMPS);
    X = SharedTensor2d(new Tensor2d("X (EF|ij)", navirA, navirA, naoccB, naoccB));
    X->sort(2413, V, 1.0, 0.0);
    Vij->gemm(false, false, bQabA, X, 1.0, 1.0);
    X.reset();
    Vij->write(psio_, PSIF_DFOCC_AMPS);
    Vij.reset();

    // V_AB^Q <= \sum_{mn} V_mAnB b_mn^Q
    X = SharedTensor2d(new Tensor2d("X (mn|AB)", naoccB, naoccB, navirA, navirA));
    X->sort(1324, V, 1.0, 0.0);
    V.reset();
    Vab = SharedTensor2d(new Tensor2d("V (Q|AB)", nQ, navirA, navirA));
    Vab->read(psio_, PSIF_DFOCC_AMPS);
    Vab->gemm(false, false, bQijB, X, 1.0, 1.0); 
    X.reset();
    Vab->write(psio_, PSIF_DFOCC_AMPS);
    Vab.reset();

    //=========================
    // V_IajB
    //=========================

    // V_IajB <= 1/2 \sum_{ME} T(IB,ME)(1) L(ME,ja)(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <IJ|AB>", naoccA, naoccA, navirA, navirA));
    U->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    T = SharedTensor2d(new Tensor2d("T2_1 (IA|JB)", naoccA, navirA, naoccA, navirA));
    T->sort(1324, U, 1.0, 0.0);
    U.reset();
    U = SharedTensor2d(new Tensor2d("T2_1 <Ij|Ab>", naoccA, naoccB, navirA, navirB));
    U->read(psio_, PSIF_DFOCC_AMPS);
    L = SharedTensor2d(new Tensor2d("T2_1 (IA|jb)", naoccA, navirA, naoccB, navirB));
    L->sort(1324, U, 1.0, 0.0);
    U.reset();
    X = SharedTensor2d(new Tensor2d("X (IB|ja)", naoccA, navirA, naoccB, navirB));
    X->gemm(false, false, T, L, 0.5, 0.0);
    T.reset();
    // V_IajB += 1/2 \sum_{me} T(IB,me)(1) L(me,ja)(1)
    U = SharedTensor2d(new Tensor2d("T2_1 <ij|ab>", naoccB, naoccB, navirB, navirB));
    U->read_anti_symm(psio_, PSIF_DFOCC_AMPS);
    T = SharedTensor2d(new Tensor2d("T2_1 (ia|jb)", naoccB, navirB, naoccB, navirB));
    T->sort(1324, U, 1.0, 0.0);
    U.reset();
    X->gemm(false, false, L, T, 0.5, 1.0);
    T.reset();
    L.reset();
    V = SharedTensor2d(new Tensor2d("V (Ia|jB)", naoccA, navirB, naoccB, navirA));
    V->sort(1432, X, 1.0, 0.0);
    X.reset();
    //V->write(psio_, PSIF_DFOCC_AMPS);

    // y_IA^Q += 2 \sum(me) V_IemA b_me^Q
    // Y(me,IA) = V(Ie,mA)
    Y = SharedTensor2d(new Tensor2d("Y (me|IA)", naoccB, navirB, naoccA, navirA));
    Y->sort(3214, V, 1.0, 0.0);
    Z = SharedTensor2d(new Tensor2d("Y (Q|IA)", nQ, naoccA, navirA));
    Z->read(psio_, PSIF_DFOCC_AMPS);
    Z->gemm(false, false, bQiaB, Y, 2.0, 1.0); 
    Y.reset();
    Z->write(psio_, PSIF_DFOCC_AMPS);
    Z.reset();

    // y_ia^Q += 2 \sum(ME) V_MaiE b_ME^Q
    // Y(ME,ia) = V(Ma,iE)
    Y = SharedTensor2d(new Tensor2d("Y (ME|ia)", naoccA, navirA, naoccB, navirB));
    Y->sort(1432, V, 1.0, 0.0);
    Z = SharedTensor2d(new Tensor2d("Y (Q|ia)", nQ, naoccB, navirB));
    Z->read(psio_, PSIF_DFOCC_AMPS);
    Z->gemm(false, false, bQiaA, Y, 2.0, 1.0); 
    Y.reset();
    Z->write(psio_, PSIF_DFOCC_AMPS);
    Z.reset();

    // Free ints
    bQijA.reset();
    bQijB.reset();
    bQiaA.reset();
    bQiaB.reset();
    bQabA.reset();
    bQabB.reset();
}// else if (reference_ == "UNRESTRICTED")

    //outfile->Printf("\tmp3_pdm_3index_intr done.\n");

}// end mp3_pdm_3index_intr

}} // End Namespaces

