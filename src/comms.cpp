#include <abcd.h>

/// Assignes each mpi-process to its category : CG-master or MUMPS-Slave
void abcd::createInterComm()
{
    mpi::communicator world;
    mpi::group grp = world.group();

    mpi::communicator cm;
    mpi::group gp;

    mpi::broadcast(world, parallel_cg, 0);

    if(parallel_cg > world.size()) throw - 14;

    inter_comm = world.split(world.rank() < parallel_cg);

    if(world.rank() < parallel_cg)
        instance_type = 0;
    else
        instance_type = 1;
}

/// Distribute the partitions over CG processes
void abcd::distributePartitions()
{
    mpi::communicator world;

    if(world.rank() == 0) {
        std::vector<int> nnz_parts;
        std::vector<int> groups;
        for(int k = 0; k < nbparts; k++) {
            nnz_parts.push_back(partitions[k].NumNonzeros());
        }

        abcd::partitionWeights(groups, nnz_parts, parallel_cg);

        // first start by the master :
        for(int i = 0; i <= groups[0] ; i++)
            parts_id.push_back(i);

        for(int i = 1; i < parallel_cg ; i++) {
            cout << "nothing to do here" << endl;
            // send to each CG-Master its starting and ending partitions ids
            std::vector<int> se;
            se.push_back(groups[i - 1]);
            se.push_back(groups[i]);

            inter_comm.send(i, 0, se);

            // send to each CG-Master the corresponding col_index and partitions
            for(int j = se[0] + 1; j <= se[1]; j++) {
                std::vector<int> sh;
                sh.push_back(partitions[j].dim(0));
                sh.push_back(partitions[j].dim(1));
                inter_comm.send(i, 1, partitions[j].NumNonzeros());
                inter_comm.send(i, 2, sh);
                inter_comm.send(i, 21, n);
                inter_comm.send(i, 3, partitions[j].colind_.t_vec(), partitions[j].NumNonzeros());
                inter_comm.send(i, 4, partitions[j].rowptr_.t_vec(), sh[0] + 1);
                inter_comm.send(i, 5, partitions[j].val_.t_vec(), partitions[j].NumNonzeros());

                inter_comm.send(i, 6, column_index[j]);
            }
        }
        cout << "sent partitions" << endl;

        // Find the interconnections between the different CG_masters
        std::vector<std::vector<int> > group_column_index;
        int st = 0;
        for(int i = 0; i < parallel_cg ; i++) {
            std::vector<int> merge_index;
            for(int j = st; j <= groups[i] ; j++) {
                std::copy(column_index[j].begin(), column_index[j].end(), back_inserter(merge_index));
            }
            std::sort(merge_index.begin(), merge_index.end());
            std::vector<int>::iterator last = std::unique(merge_index.begin(), merge_index.end());
            merge_index.erase(last, merge_index.end());
            group_column_index.push_back(merge_index);
            st = groups[i] + 1;

        }

        cout << "Merge done" << endl;

        // Send those interconnections to the other masters
        std::map<int, std::vector<int> > inter;
        for(int i = 0; i < parallel_cg; i++) {
            for(int j = i + 1; j < parallel_cg; j++) {
                std::vector<int> inter1;
                std::vector<int> inter2;
                std::vector<int>::iterator it1 = group_column_index[i].begin();
                std::vector<int>::iterator it2 = group_column_index[j].begin();

                while(it1 != group_column_index[i].end() && it2 != group_column_index[j].end()) {
                    if(*it1 < *it2) {
                        ++it1;
                    } else {
                        if(!(*it2 < *it1)) {
                            inter1.push_back(
				    it1 - group_column_index[i].begin()
                            );
                            inter2.push_back(
				    it2 - group_column_index[j].begin()
                            );

                            inter[i].push_back(
				    it1 - group_column_index[i].begin()
                            );
                            inter[j].push_back(
				    it1 - group_column_index[i].begin()
                            );
                        }
                        ++it2;
                    }
                }
                if(i == 0) {
                    col_interconnections[j] = inter1;
                } else {
                    inter_comm.send(i, 7, j);
                    inter_comm.send(i, 8, inter1);
                }
                inter_comm.send(j, 7, i);
                inter_comm.send(j, 8, inter2);
            }
        }


        for(int i = 1; i < parallel_cg; i++) {
            inter_comm.send(i, 7, -1);
            std::map<int, int> l_glob_to_local;
            for(int j = 0; j < group_column_index[i].size(); j++) {
                l_glob_to_local[group_column_index[i][j]] = j;
            }
        }

        std::map<int, int> l_glob_to_local;
        for(int j = 0; j < group_column_index[0].size(); j++) {
            l_glob_to_local[group_column_index[0][j]] = j;
        }

        glob_to_local = l_glob_to_local;

        cout << "sent interconnections to others" << endl;


        // Now that everybody got its partition, delete them from the master
        partitions.erase(partitions.begin() + groups[0] + 1, partitions.end());
        column_index.erase(column_index.begin() + groups[0] + 1, column_index.end());

        m_l = m;
        m = std::accumulate(partitions.begin(), partitions.end(), 0, sum_rows);
        nz = std::accumulate(partitions.begin(), partitions.end(), 0, sum_nnz);
    } else {
        std::vector<int> se;
        inter_comm.recv(0, 0, se);
        int sm = 0, snz = 0;
        for(int i = se[0] + 1; i <= se[1]; i++) {

            // The partition number to be added
            parts_id.push_back(i);

            // THe partition data
            std::vector<int> sh;
            int l_nz, l_m, l_n;
            inter_comm.recv(0, 1, l_nz);
            inter_comm.recv(0, 2, sh);
            inter_comm.recv(0, 21, n);
            l_m = sh[0];
            l_n = sh[1];

            int *l_jcn = new int[l_nz];
            int *l_irst = new int[l_m + 1];
            double *l_v = new double[l_nz];

            inter_comm.recv(0, 3, l_jcn, l_nz);
            inter_comm.recv(0, 4, l_irst, l_m + 1);
            inter_comm.recv(0, 5, l_v, l_nz);

            // Continue the communications and let the initialization of the matrix at the end
            std::vector<int> ci;
            inter_comm.recv(0, 6, ci);
            column_index.push_back(ci);

            // Create the matrix and push it in!
            CompRow_Mat_double mat(l_m, l_n, l_nz, l_v, l_irst, l_jcn);
            partitions.push_back(mat);

            sm += l_m;
            snz += l_nz;
        }

        while(true) {
            int the_other;
            std::vector<int> inter;
            inter_comm.recv(0, 7, the_other);
            if(the_other == -1) break;
            inter_comm.recv(0, 8, inter);
            col_interconnections[the_other] = inter;
        }


        // Set the number of rows and nnz handled by this CG Instance
        m = sm;
        nz = snz;
        cout << "rececption finished" << endl;
    }

    // Create a merge of the column indices
    std::vector<int> merge_index;
    for(int j = 0; j < partitions.size(); j++) {
        std::copy(column_index[j].begin(), column_index[j].end(), back_inserter(merge_index));
    }
    std::sort(merge_index.begin(), merge_index.end());
    std::vector<int>::iterator last = std::unique(merge_index.begin(), merge_index.end());
    merge_index.erase(last, merge_index.end());


    // for each partition find a local column index for the previous merge
    int indices[partitions.size()];
    for(int i = 0; i < partitions.size(); i++) indices[i] = 0;

    local_column_index = std::vector<std::vector<int> >(partitions.size());

    for(int j = 0; j < merge_index.size(); j++) {
        for(int i = 0; i < partitions.size(); i++) {
            if(column_index[i][indices[i]] == merge_index[j] &&
                    indices[i] < column_index[i].size() ) {

                local_column_index[i].push_back(j);
                indices[i]++;
            }
        }
    }

    n = merge_index.size();

    // Create a Communication map for ddot
    comm_map.assign(n, 1);
    for(std::map<int, std::vector<int> >::iterator it = col_interconnections.begin();
            it != col_interconnections.end(); it++) {
        if(inter_comm.rank() > it->first) {
            for(std::vector<int>::iterator i = it->second.begin(); i != it->second.end(); i++) {
                if(comm_map[*i] == 1) comm_map[*i] = -1;
            }
        }
    }
    nrmA = 0;
    for(int i=0; i<partitions.size(); i++)
        nrmA += squaredNorm(partitions[i]);

    mpi::all_reduce(inter_comm, &nrmA, 1,  &nrmMtx, std::plus<double>());
    nrmA = sqrt(nrmA);
    nrmMtx = sqrt(nrmMtx);
}

void abcd::distributeRhs()
{
    mpi::communicator world;

    mpi::broadcast(inter_comm, use_xf, 0);

    if(world.rank() == 0) {

        int r_pos = 0;
        // Build my part of the RHS
        int r = std::accumulate(partitions.begin(), partitions.end(), 0, sum_rows);

        if(rhs==NULL){
            rhs = new double[r * nrhs];
            for(int i=0; i<r*nrhs; i++) rhs[i] = ((double)rand()/(double)RAND_MAX);
        }

        if(use_xf){
            xf = Eigen::MatrixXd(mtx.cols(), nrhs);
            xf.setZero();
            for(int j = 0; j < nrhs; j++){
                for(int i = 0; i < mtx.cols(); i++) {
                    xf(i, j) = rhs[i + j * m];
                }
            }
            nrmXf = xf.lpNorm<Infinity>();
            Eigen::MatrixXd B = mtx * xf;

            // this is the augmented version!
            if(icntl[10]!=0){
                Eigen::MatrixXd xtf = xf;
                Eigen::MatrixXd xtz(n-mtx.cols(), nrhs);
                xtz.setZero();

                xf.resize(n, nrhs);
                xf << xtf,
                   xtz;
            }

            for(int j = 0; j < B.cols(); j++){
                for(int i = 0; i < B.rows(); i++) {
                    rhs[i + j * B.rows()] = B(i, j);
                }
            }
        }

        b = Eigen::MatrixXd(r, nrhs);
        for(int j = 0; j < nrhs; j++){
            for(int i = 0; i < r; i++) {
                b(i, j) = rhs[i + j * m];
            }
        }

        r_pos += r;

        // for other masters except me!
        for(int k = 1; k < parallel_cg; k++) {
            // get the partitions that will be sent to the master
            int rows_for_k;
            inter_comm.recv(k, 16, rows_for_k);
            inter_comm.send(k, 17, nrhs);
            for(int j = 0; j < nrhs; j++) {
                inter_comm.send(k, 18, rhs + r_pos + j * m, rows_for_k);
            }

            r_pos += rows_for_k;
        }
    } else {
        inter_comm.send(0, 16, m);
        inter_comm.recv(0, 17, nrhs);

        b = Eigen::MatrixXd(m, nrhs);
        b.setZero();

        rhs = new double[m * nrhs];
        for(int i = 0; i < m * nrhs; i++) rhs[i] = 0;

        for(int j = 0; j < nrhs; j++) {

            inter_comm.recv(0, 18, rhs + j * m, m);

            for(int i = 0; i < m; i++) {
                b(i, j) = rhs[i + j * m];
            }

        }
    }
    
    // and distribute max iterations
    mpi::broadcast(inter_comm, itmax, 0);
}






















