// A simple working example of
// =======================


// We include the ABCD solver header file, it contains the `abcd` class definition
#include "abcd.h"

// Use `boost::mpi` for simplicity, the user can use which ever MPI library he wants
#include "mpi.h"
#include <boost/mpi.hpp>

// A simple matrix generator for a regular 2D mesh + 5-point stencil 
void init_2d_lap(int m, int n, int nz, int *irn, int *jcn, double *val, int mesh_size);
void init_2d_lap(abcd &o, int mesh_size);

int main(int argc, char* argv[]) 
{
    // Equivalent to MPI_Initialize
    mpi::environment env(argc, argv);

    // obtain the WORLD communicator, by default the solver uses it
    mpi::communicator world;

    // To see the use of sub-communicator in action, we split the
    // world communicator in even/odd communicators, and use
    // only the even ones
    bool is_even = world.rank() % 2 == 0;
    mpi::communicator solver_comm = world.split(is_even ? 0 : 1);

    if (is_even) {
        // create one instance of the abcd solver per mpi-process
        abcd obj;

        obj.comm = solver_comm;
        obj.icntl[Controls::verbose_level] = 1;
        
        if(solver_comm.rank() == 0) { // the master
            init_2d_lap(obj, 100);

            // set the rhs
            obj.rhs = new double[obj.m];
            for (size_t i = 0; i < obj.m; i++) {
                obj.rhs[i] = ((double) i + 1)/obj.m;
            }
        }

        try {
            obj(-1);
            obj(6); // equivalent to running 1, 2 and 3 successively
        } catch (runtime_error err) {
            cout << "An error occured: " << err.what() << endl;
        }
    }

    return 0;
}

void init_2d_lap(abcd &obj, int mesh_size)
{
    obj.m = mesh_size*mesh_size; // number of rows
    obj.n = obj.m; // number of columns
    obj.nz = 3*obj.m - 2*mesh_size; // number of nnz in the lower-triangular part
    obj.sym = true;

    // allocate the arrays
    obj.irn = new int[obj.nz];
    obj.jcn = new int[obj.nz];
    obj.val = new double[obj.nz];

    init_2d_lap(obj.m, obj.n, obj.nz, obj.irn, obj.jcn, obj.val, mesh_size);
}

void init_2d_lap(int m, int n, int nz, int *irn, int *jcn, double *val, int mesh_size)
{
    int pos;
    int i;

    pos = 0;
    for (i = 1; i <= m; i++) {

        // the diagonal
        irn[pos] = i;
        jcn[pos] = i;
        val[pos] = -4.0;

        pos++;

        if (i % mesh_size != 0) {
            // the lower-triangular part
            irn[pos] = i + 1;
            jcn[pos] = i ;
            val[pos] = 1.0;
            pos++;
        } 

        if (i + mesh_size > m) continue;
        irn[pos] = i + mesh_size ;
        jcn[pos] = i ;
        val[pos] = 1.0;
        pos++;
    }
}
