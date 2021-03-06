// Copyright Institut National Polytechnique de Toulouse (2014) 
// Contributor(s) :
// M. Zenadi <mzenadi@enseeiht.fr>
// D. Ruiz <ruiz@enseeiht.fr>
// R. Guivarch <guivarch@enseeiht.fr>

// This software is governed by the CeCILL-C license under French law and
// abiding by the rules of distribution of free software.  You can  use, 
// modify and/ or redistribute the software under the terms of the CeCILL-C
// license as circulated by CEA, CNRS and INRIA at the following URL
// "http://www.cecill.info/licences/Licence_CeCILL-C_V1-en.html"

// As a counterpart to the access to the source code and  rights to copy,
// modify and redistribute granted by the license, users are provided only
// with a limited warranty  and the software's author,  the holder of the
// economic rights,  and the successive licensors  have only  limited
// liability. 

// In this respect, the user's attention is drawn to the risks associated
// with loading,  using,  modifying and/or developing or reproducing the
// software by the user in light of its specific status of free software,
// that may mean  that it is complicated to manipulate,  and  that  also
// therefore means  that it is reserved for developers  and  experienced
// professionals having in-depth computer knowledge. Users are therefore
// encouraged to load and test the software's suitability as regards their
// requirements in conditions enabling the security of their systems and/or 
// data to be ensured and,  more generally, to use and operate it in the 
// same conditions as regards security. 

// The fact that you are presently reading this means that you have had
// knowledge of the CeCILL-C license and that you accept its terms.

#include <abcd.h>
#include "blas.h"
#include "mat_utils.h"

using namespace std;
using namespace boost::lambda;

/// Partition weigts
void abcd::partitionWeights(std::vector<std::vector<int> > &parts, std::vector<int> weights, int nb_parts)
{
    std::vector<int> sets(nb_parts);
    std::map<int, std::vector<int> > pts;

    for(int i = 0; i < nb_parts; i++){
        sets[i] = 0;
        pts[i];
    }

    if (nb_parts == (int)weights.size()) {
        for(int i = 0; i < nb_parts; i++)
            pts[i].push_back(i);
    } else {
        int avg = accumulate(weights.begin(), weights.end(), 0);
        avg = floor(0.9 * (double)avg / nb_parts);

        float fix = 1.0;
        int weight_index = weights.size() + 1;
        while (weight_index > (int)weights.size()){
            weight_index = 0;
            pts.clear();
            int current_partition = 0;
            int current_weight = 0;

            // Share everything sequentially
            while( (current_partition < nb_parts) &&
                   (weight_index < (int)weights.size())) {

                if((weights[weight_index] > avg * (fix - 0.25))
                    && (pts[current_partition].size() != 0)){
                    current_partition++;
                    current_weight = 0;
                } else {
                    current_weight += weights[weight_index];
                    pts[current_partition].push_back(weight_index);
                    weight_index++;
                    if(current_weight > avg * fix){
                        current_partition++;
                        current_weight = 0;
                    }
                }
            }
            fix -= 0.25;
        }

        // if there are some weights left, go greedy
        if (weight_index < (int)weights.size()) {
            int cur = weight_index;
            int ls = 0;
            while(cur != (int)weights.size()){
                int sm = ls;
                for(int i = 0; i < nb_parts; i++){
                    if(sets[i] < sets[sm]) sm = i;
                }
                sets[sm] = sets[sm] + weights[cur];
                pts[sm].push_back(cur);
                cur++;
                ls = sm;
            }
        }
    }

    for(int i = 0; i < nb_parts; i++){
        parts.push_back(pts[i]);
    }

}

///DDOT
double abcd::ddot(VECTOR_double &p, VECTOR_double &ap)
{
    int lm = p.size();
    int rm = ap.size();
    if(lm != rm) throw - 800;

    VECTOR_double loc_p(lm, 0);
    VECTOR_double loc_ap(rm, 0);
    double loc_r = 0, r = 0;

    int pos = 0;
    for(int i = 0; i < lm; i++) {
        if(comm_map[i] == 1) {
            loc_p(pos) = p(i);
            loc_ap(pos) = ap(i);
            pos++;
        }
    }

    // R = P'AP
    for(int i = 0; i < lm; i++){
        loc_r += loc_p(i) * loc_ap(i);
    }

    mpi::all_reduce(inter_comm, loc_r, r, std::plus<double>());

    return r;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  abcd::get_nrmres
 *  Description:  Computes ||X_k|| and ||X_f - X_k||/||X_f||
 * =====================================================================================
 */
void abcd::get_nrmres(MV_ColMat_double &x, MV_ColMat_double &b,
                      VECTOR_double &nrmR, VECTOR_double &nrmX)
{
    int rn = x.dim(1);
    int rm = x.dim(0);

    nrmX = 0;
    nrmR = 0;

    VECTOR_double nrmXV(rn, 0);
    VECTOR_double nrmRV(rn, 0);

    MV_ColMat_double loc_x(rm, rn, 0);
    MV_ColMat_double loc_r(m, rn, 0);
    MV_ColMat_double loc_xfmx(rm, rn, 0);

    int pos = 0;
    for(int i = 0; i < rm; i++) {
        if(comm_map[i] == 1) {
            for(int j = 0; j < rn; j++) {
                nrmXV(j) += abs(x(i, j));
            }
            pos++;
        }
    }

    pos = 0;


    for(int p = 0; p < nb_local_parts; p++) {
        for(int j = 0; j < rn; j++) {
            VECTOR_double compressed_x = VECTOR_double((partitions[p].dim(1)), 0);

            int x_pos = 0;
            for(size_t i = 0; i < local_column_index[p].size(); i++) {
                int ci = local_column_index[p][i];
                compressed_x(x_pos) = x(ci, j);

                x_pos++;
            }
            VECTOR_double vj =  partitions[p] * compressed_x;
            int c = 0;
            for(int i = pos; i < pos + partitions[p].dim(0); i++)
                loc_r(i, j) = vj[c++];
        }

        pos += partitions[p].dim(0);
    }

    loc_r  = b - loc_r;

    for(int j = 0; j < rn; j++){
        VECTOR_double loc_r_j = loc_r(j);
        nrmRV(j) = infNorm(loc_r_j);
    }

    mpi::all_reduce(inter_comm, nrmRV.ptr(), rn, nrmR.ptr(), mpi::maximum<double>());
    mpi::all_reduce(inter_comm, nrmXV.ptr(), rn, nrmX.ptr(), std::plus<double>());

}

double or_bin(double &a, double &b){
    if(a!=0) return a;
    else if(b!=0) return b;
    else return 0;
}

std::vector<int> sort_indexes(const int *v, const int nb_el) {

    typedef std::pair<int,int> pair_type;
    std::vector< std::pair<int,int> > vp;
    vp.reserve(nb_el);

    for(int i = 0; i < nb_el; i++)
        vp.push_back( std::make_pair<int,int>(v[i], i) );


    // sort indexes based on comparing values in v
    sort(vp.begin(), vp.end(), bind(&pair_type::first, _1) < bind(&pair_type::first, _2));

    std::vector<int> idx(vp.size());
    transform(vp.begin(), vp.end(), idx.begin(), bind(&pair_type::second, _1));
    return idx;
}

/// Regroups the data from the different sources to a single destination on the master
void abcd::centralizeVector(double *dest, int dest_lda, int dest_ncols,
                            double *src,  int src_lda,  int src_ncols,
                            std::vector<int> globalIndex, double *scale)
{
    if (src_ncols != dest_ncols) {
        throw std::runtime_error("Source's number of columns must be the same as the destination's");
    }

    MV_ColMat_double source(src, src_lda, src_ncols, MV_Matrix_::ref);
    
    if(IRANK == 0) {
        double t = MPI_Wtime();

        MV_ColMat_double vdest(dest, dest_lda, dest_ncols, MV_Matrix_::ref);

        std::map<int, std::vector<double> > xo;
        std::map<int, std::vector<int> > io;
        std::map<int, int > lo;

        for(int k = 1; k < inter_comm.size(); k++){
            inter_comm.recv(k, 71, xo[k]);
            inter_comm.recv(k, 72, io[k]);
            inter_comm.recv(k, 73, lo[k]);
        }
        for(int k = 1; k < inter_comm.size(); ++k){
            for(int j = 0; j < dest_ncols; ++j)
                for(size_t i = 0; i < io[k].size() && io[k][i] < n_o; ++i){
                    int ci = io[k][i];
                    vdest(ci, j) = xo[k][i + j * lo[k]] * scale[ci];
                }
        }
        
        for(int j = 0; j < dest_ncols; ++j)
            for(size_t i = 0; i < globalIndex.size() && glob_to_local_ind[i] < n_o; ++i){
                vdest(globalIndex[i], j) = source(i, j) * dcol_[globalIndex[i]];
            }

        ///@TODO Move this away
        if(Xf.dim(0) != 0) {
            MV_ColMat_double xf =  Xf - vdest;
            double nrmxf =  infNorm(xf);
            dinfo[Controls::forward_error] =  nrmxf/nrmXf;
        }
    } else {
        std::vector<double> x(src, src + src_lda * src_ncols);

        inter_comm.send(0, 71, x);
        inter_comm.send(0, 72, glob_to_local_ind);
        inter_comm.send(0, 73, src_lda);
    }
}
