/*
 * abcd.h
 *
 *  Created on: Aug 15, 2012
 *      Author: Mohamed Zenadi
 */

#ifndef ABCD_HXX_
#define ABCD_HXX_

#include <iostream>
#include <iomanip>
#include <numeric>
#include <vector>
#include <cstdio>
#include <algorithm>
#include "mmio.h"
#include "mpi.h"

#include "dmumps_c.h"

#include <boost/mpi.hpp>
#include <boost/accumulators/statistics/mean.hpp>

#include <boost/progress.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_sparse.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>

/*
 * A small hack to make Sparselib++ work with openmpi
 * It's however, better to use it while compiling by putting:
 * -D"COMPLEX=std::complex<double>"
 */
#define COMPLEX std::complex<double>
#include "compcol_double.h"
#include "comprow_double.h"
#include "coord_double.h"
#include "mvm.h"

#include "splib_utils.h"

using namespace std;
using namespace boost;
using namespace boost::numeric;
using namespace ublas;

class abcd
{
private:
    // Types to be used localy
    double nrmA;
    double nrmB;
    double nrmXf;
    double nrmMtx;

    void initialize();

    // preprocess stuffs
    void preprocess();
    /**
     * Scales the matrix
     * @norm the norm at which the matrix is scaled
     */
    void scaleMatrix(int norm);
    void diagScaleMatrix(VECTOR_double , VECTOR_double );
    void diagScaleRhs(VECTOR_double &);
    /**
     * Computes the norm of the matrix
     * @todo implement it!
     */
    void computeNorms();

    // structure functions
    /// Partitions the matrix into abcd::nbrows
    void partitionMatrix();
    /**
     * Analyses the structure of each partition
     * Compresses the partitions and analyses the interconnections between them
     */
    void analyseFrame();
    
    /*-----------------------------------------------------------------------------
     *  Build the augmented version of the matrix (ABCD)
     *-----------------------------------------------------------------------------*/
    void augmentMatrix(std::vector<CompCol_Mat_double > &loc_parts);

    // Communication stuffs
    void createInterComm();
    void distributePartitions();

    // Cimmino
    void initializeCimmino();
    void distributeRhs();
    void bcg(MV_ColMat_double &b);
    void solveABCD(MV_ColMat_double &b);
    Coord_Mat_double buildS();
    int gqr(MV_ColMat_double &P, MV_ColMat_double &AP, MV_ColMat_double &R, int s, bool use_a);
    int gqr(MV_ColMat_double &p, MV_ColMat_double &ap, MV_ColMat_double &r, CompCol_Mat_double g, int s, bool use_a);
    void gmgs(MV_ColMat_double &p, MV_ColMat_double &ap, MV_ColMat_double &r, int s, bool use_a);
    void gmgs(MV_ColMat_double &p, MV_ColMat_double &ap, MV_ColMat_double &r, CompCol_Mat_double g, int s, bool use_a);
    double compute_rho(MV_ColMat_double &X, MV_ColMat_double &U, double thresh);
    std::vector<double> normres;
    int size_c;

    // MUMPS
    int m_n;
    int m_nz;
    DMUMPS_STRUC_C mumps;
    void initializeMumps(bool local);
    void initializeMumps();
    void createAugmentedSystems();
    void analyseAugmentedSystems();
    void allocateMumpsSlaves();
    void factorizeAugmentedSystems();
    std::vector<int> my_slaves;
    int my_master;
    MV_ColMat_double sumProject(double alpha, MV_ColMat_double &Rhs, double beta, MV_ColMat_double &X);
    void waitForSolve();
    std::vector<int> comm_map;

    // MUMPS setters and getters
    inline void setMumpsIcntl(int i, int v) { mumps.icntl[ i - 1 ] = v ; }
    inline void setMumpsCntl(int i, double v) { mumps.cntl[ i - 1 ] = v ; }
    inline int getMumpsInfo(int i) { return mumps.info[ i - 1 ]; }
    inline double getMumpsRinfo(int i) { return mumps.rinfo[ i - 1 ]; }
    inline double getMumpsRinfoG(int i) { return mumps.rinfog[ i - 1 ]; }

    // SOme utilities
    void partitionWeights(std::vector<int> &, std::vector<int>, int);
    double ddot(VECTOR_double &p, VECTOR_double &ap);
    void get_nrmres(MV_ColMat_double &x, double &nrmR, double &nrmX, double &nrmXfmX);


    /*
     * Scaling information
     */
    VECTOR_double drow_;
    VECTOR_double dcol_;

    /***************************************************************************
     * The matrix object itself
    ***************************************************************************/
    Coord_Mat_double S;
    std::vector<CompRow_Mat_double> partitions;
    MV_ColMat_double Xk;

    MV_ColMat_double Xf;
    MV_ColMat_double B;
    MV_ColMat_double RRand;

    CompRow_Mat_double A;

public:
    /***************************************************************************
     * Matrix information
     */
    int m;
    int n;
    int nz;
    int nrhs;
    int n_l, m_l, nz_l;
    int n_o, m_o, nz_o;


    /***************************************************************************
     * Temporary data about the matrix
    ***************************************************************************/
    int *irn;
    int *jcn;
    double *val;
    double *rhs;

    /***************************************************************************
     * Matrix properties
    ***************************************************************************/
    bool sym; /// Symmetry
    int start_index; /// To define wether it's Fortran-Style (1) or C-Style (0)

    /***************************************************************************
     * Partitioning informations
    ***************************************************************************/

    /**
     * Defines the type of partitioning :
     * * 1 for manual partitioning (nbparts and nbrows are suplied)
     * * 2 for automatic partitioning (nbparts only is needed)
     */
    int partitioning_type;
    int nbparts; /// The number of partitions
    VECTOR_int strow; /// The starting row index of each partition
    VECTOR_int nbrows; /// The number of rows per partition
    /// A reverse index of columns, contains the original index of each column for each partition
    std::vector<std::vector<int> > column_index;
    /// A merge of col_index vectors, determines non-null columns in all local partitions
    std::vector<std::vector<int> > local_column_index;
    std::map<int,int> glob_to_local;
    bool use_xk;
    bool use_xf;

    /**
     * Contains the mutual interconnections between partitions
     * The key is the cg-master rank (in inter_comm) and the value is the column indices
     */
    std::map<int,std::vector<int> > col_interconnections;
    /// Contains the partitions that are handled by this instance
    std::vector<int> parts_id;

    int block_size;
    int itmax;

    bool verbose;


    /***************************************************************************
     * Communication info
    ***************************************************************************/
    int instance_type; /// The type of process : 0 for CG, 1 for MUMPS Slave
    int parallel_cg; /// The number of parallel CG instances

    mpi::communicator inter_comm; /// The communicator shared by CG masters
    mpi::communicator intra_comm; /// The communicator of local slaves


    int icntl[20];
    double dcntl[20];


    int bc(int);
    abcd();
    ~abcd();

};

typedef std::pair<double,int> dipair;
bool ip_comp(const dipair &, const dipair &);

#endif // ABCD_HXX
