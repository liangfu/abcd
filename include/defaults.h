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

#ifndef _DEFAULTS_H_
#define _DEFAULTS_H_
///\xrefitem exp "Experimental" "Experimental List

/// Defines the control parameters indices in a safe way
namespace Controls{
    //! To be used with the abcd::icntl vector.
    enum icontrols {
        /*! \brief Number of partitions
         *
         * Defines the number of partitions in our linear system,
         * can be from ``1`` to ``m`` (the number of rows in the matrix)
         *
         * \rststar
         *      .. code-block:: cpp
         *
         *          // we have 8 partitions
         *          obj.icntl[nbparts] = 8;
         * \endrststar
         */
        nbparts            ,

        /*! \brief Partitioning strategy
         *
         * Defines the partitioning strategy, it can have the values:
         * - 1, Manual partitioning, the nbparts partitions can be
         *   provided in the STL vector nbrows. Example:
         * \rststar
         *     .. code-block:: cpp
         *
         *         // use manual partitioning
         *         obj.icntl[part_type] = 1;
         *         // say that we want 20 rows per partition
         *         obj.nrows.assign(obj.icntl[nbparts], 20);
         *
         *         // or
         *         obj.nrows.resize(obj.icntl[nbparts]);
         *         obj.nrows[0] = 20;
         *         obj.nrows[1] = 20;
         *         //...
         *
         *     For C, the ``nrows`` vector is an ``int`` array:
         *
         *     .. code-block:: cpp
         *
         *
         *         // use manual partitioning
         *         obj->icntl[part_type] = 1;
         *
         *         obj->nrows =  (int*) malloc(sizeof(int)*(obj->icntl[nbparts]));
         *
         *         obj->nrows[0] = 20;
         *         obj->nrows[1] = 20;
         *         //...
         *
         * \endrststar
         *
         * - 2, (*default*) Automatic uniform partitioning, creates
         *    *nbparts* partitions of similar size.
         * \rststar
         *     .. code-block:: cpp
         *
         *
         *         // use patoh partitioning
         *         obj.icntl[part_type] = 2;
         * \endrststar
         *
         * - 3, Automatic hypergraph partitioning, creates *nbparts*
         *   partitions using the hypergraph partitioner
         *   ``PaToH``. The imbalance between the partitions is
         *   handled using ``obj.dcntl[part_imbalance]``. Example:
         * \rststar
         *     .. code-block:: cpp
         *
         *
         *         // use patoh partitioning
         *         obj.icntl[part_type] = 3;
         *         // say that we want an imbalance factor up to 30% between the partitions
         *         obj.dcntl[part_imbalance] = 0.3;
         *
         * \endrststar
         */
        part_type          ,

        /*! \brief Guess the number of partitions
         *
         * Asks the solver to guess the appropriate number of
         * partitions and overrides the defined *nbparts*.
         *
         * - 0 (*default*), The user has to provide the number of
         * partitions by setting icntl[nbparts]
         * - 1, Guess the number of partitions by trying to create a
         *   small number of partitions while keeping them small
         *   enough to be handled easily by the direct solver
         */
        part_guess         ,

        /*! \brief The scaling type
         *
         * Defines the type of scaling to be used.
         * - 0, no scaling
         * - 1, scale the input matrix (embeded internal scaling strategy)
        */
        scaling            ,

        /*! \brief The max number of iterations
         *
         * Defines the maximum number of iterations in block-CG
         * acceleration, default is ``1000`` iterations.
         */
        itmax              ,

        /*! \brief Block-CG block-size
         *
         * Defines the block-size to be used by the block-CG
         * acceleration, default is ``1`` for classical CG
         * acceleration. When using a higher value than 1, 
         * stabilized Block-CG is used.
         */
        block_size         ,

        /*! \brief The verbose level
         *
         * Defines the verbosity of the solver
         */
        verbose_level      , 
        
        /*! \brief The augmentation type
         *
         * Possible values are:
         *  - ``0`` (*default*), no augmentation. This makes the solver run in
         *  **regular block Cimmino** mode.
         *
         *  - ``1``, makes the solver run in **Augmented Block
         *  Cimmino** mode with an augmentation of the matrix using
         *  the \f$C_{ij}\f$ augmentation strategy. For numerical
         *  stability, this augmentation technique has to be used with
         *  a scaling.
         *
         *  - ``2``, makes the solver run in **Augmented Block
         *  Cimmino** mode with an augmentation of the matrix using
         *  the \f$A_{ij}\f$ augmentation starategy.
         */
        aug_type            ,

        /*! \brief The blocking factor in ABCD
         *
         *
         * Defines the blocking factor used by the solver during the
         * solution phase, its default value is 128. This allows the
         * solver to take advantage of BLAS3 Kernels.  The optimal
         * value is hardware and problem size related. The user has
         * also to find a compromise between memory and computing
         * time.
         */
        aug_blocking        ,

#ifdef WIP
        /*! \brief Exploit the sparcity in MUMPS
         */
        exploit_sparcity    ,

        /*! \brief Force Gram-Schmidt with reorthogonalization in Block-CG
         *
         * Makes the Block-CG use the Modifed Gram-Schmidt with
         * reorthogonalization rather than QR factorization during the
         * stabilization process. The :math:`GMGS^2` algorithm is described by
         * Björk in *Numerical Methods for Least Squares Problems*.
         * This option is useful only if #block_size is greater than 1.
         *
         * - 0 (*default*), Use QR factorization
         * - 1, Use Modified Gram-Schmidt with reorthogonalization
         */
        use_gmgs2          ,

        /*! \brief Analyse the augmentation process
         *
         * When set to a value different than ``0``, analyses the
         * number of columns in the augmentation process.
         * **Note**: This does not build \f$S\f$, but only augment the matrix.
         */
        aug_analysis        ,

        aug_iterative   , ///< \exp Enable or disable iterative solving of Sz=f

        aug_project     , ///< \deprecated Compute the projection only
        aug_dense       , ///< \deprecated Use dense RHS when doing the computation
#endif //WIP
    };
    enum dcontrols {
        part_imbalance, ///< The imbalance factor in PaToH case
        threshold     , ///< The stoping threshold

#ifdef WIP
        aug_filter    , ///< \deprecated The filtering value
        aug_precond   , ///< \exp The preconditioner criteria of selection
#endif //WIP
    };
    enum info {
        status        , ///< Exit status
        nb_iter       , ///< Number of iterations after CG
    };

    enum dinfo {
        residual       , ///< The resulting residual
        forward_error  , ///< The resulting forward error
        backward       , ///< The resulting residual
        scaled_residual, ///< The resulting residual
    };

}

#endif // _DEFAULTS_H_
