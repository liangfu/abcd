#include <abcd.h>

using namespace std;

/// The gateway function that launches all other options
int abcd::bc(int job)
{
    mpi::communicator world;

    switch(job) {

    case -1:
        if(world.rank() == 0) {
            abcd::initialize();
            n_o = n;
            m_o = m;
            nz_o = nz;
        }
        world.barrier();
        break;

    case 1:
        if(world.rank() == 0) {
            abcd::preprocess();
            abcd::partitionMatrix();
            abcd::analyseFrame();
        }

        world.barrier();
        break;

    case 2:
        // Create the group of CG instances
        abcd::createInterComm();

        if(instance_type == 0) {
            // some data on augmented system
            mpi::broadcast(inter_comm, m_o, 0);
            mpi::broadcast(inter_comm, n_o, 0);
            mpi::broadcast(inter_comm, nz_o, 0);
            mpi::broadcast(inter_comm, icntl[10], 0);
            if( icntl[10] != 0 )
                mpi::broadcast(inter_comm, size_c, 0);
        }

        if(instance_type == 0) {
            abcd::distributePartitions();
        }
        abcd::initializeCimmino();
        if(inter_comm.rank() == 0 && instance_type == 0){
            cout << "[+] Launching MUMPS factorization" << endl;
        }
        abcd::factorizeAugmentedSystems();
        break;

    case 3:
        if(inter_comm.rank() == 0 && instance_type == 0){
            cout << "[+] Launching Solve" << endl;
        }
        if(instance_type == 0) {
            inter_comm.barrier();
            abcd::distributeRhs();
            if(icntl[10] == 0){
                abcd::bcg(B);
            } else{
                abcd::solveABCD(B);
            }
        } else {
            abcd::waitForSolve();
        }
        world.barrier();
        break;

    default:
        // Wrong job id
        return -1;
    }

    return 0;
}

/// Creates and intializes the matrix object
void abcd::initialize()
{

    // Check that the matrix data is present
    if(irn == 0 ||
            jcn == 0 ||
            val == 0) {

        // Hey!! where is my data?
        throw - 1;
    }

    double t = MPI_Wtime();
    Coord_Mat_double t_A;

    if(sym) {
        //over estimate nz
        int     *t_irn = new int[2*nz];
        int     *t_jcn = new int[2*nz];
        double  *t_val = new double[2*nz];
        int     t_nz = 0;
        for(int k = 0; k < nz; ++k) {
            irn[k]--; jcn[k]--;

            t_irn[t_nz] = irn[k];
            t_jcn[t_nz] = jcn[k];
            t_val[t_nz] = val[k];
            t_nz++;
            if(irn[k] != jcn[k]){
                t_irn[t_nz] = jcn[k];
                t_jcn[t_nz] = irn[k];
                t_val[t_nz] = val[k];
                t_nz++;
            }
        }
        nz = t_nz;
        t_A = Coord_Mat_double(m, n, t_nz, t_val, t_irn, t_jcn);
    } else {
        for(int i=0; i<nz; i++){
            irn[i]--;
            jcn[i]--;
        }
        t_A = Coord_Mat_double(m, n, nz, val, irn, jcn);
    }
    A = CompRow_Mat_double(t_A);
    cout << "splib : " << MPI_Wtime() - t << endl;
}


/// Set the defaults in the constructor
abcd::abcd()
{
    nrhs = 1;
    start_index = 0;
    block_size = 1;
    use_xk = false;
    use_xf = false;
    rhs = NULL;
    size_c = 0;
    verbose = false;
    for(int i = 0; i < 20; i++) icntl[i] = 0;
    for(int i = 0; i < 20; i++) dcntl[i] = 0;
}

abcd::~abcd()
{
}
