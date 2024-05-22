/************************************************************************************************************************
 * In these two header files, we include:
 *    - Typedefs:   used to create an additional name for another data type, but does not create a new type.
 *    - Type alias: same as typedefs, but allows for templates, e.g. Vector<f64>, Vector<i32>.
 ************************************************************************************************************************/
#include "CoreIncludes.hpp"
#include "mesh/mesh.hpp"
#include "mesh/valueSource.hpp"

#include <vector>
#include <fstream>
#include <iostream>

/************************************************************************************************************************
 * Solve -div(grad(u)) = f, using FDM
 ************************************************************************************************************************/
int main(){

    //## ================== ##//
    //## Provide parameters ##//
    //## ================== ##//
    const u32 imax  = 1001;              /**< #gridpoints in x */
    const u32 jmax  = 1001;              /**< #gridpoints in y */
    const f64 Lx[2] = {0., 1.*EIGEN_PI}; /**< domain endpoints in x */
    const f64 Ly[2] = {0., 1.*EIGEN_PI}; /**< domain endpoints in y */

    //## ==================== ##//
    //## Calculate parameters ##//
    //## ==================== ##//
    const u32 n = imax*jmax;     /**< sparse matrix size component (n,n), boundaries included */

    //## ============= ##//
    //## Problem Setup ##//
    //## ============= ##//
    // Declare and initalise 2D gridpoint list
    Mesh::gridStruct grid;
    grid.x.setLinSpaced(imax, Lx[0], Lx[1]);
    grid.y.setLinSpaced(jmax, Ly[0], Ly[1]);

    // Declare and initialise boundary condition list
    Mesh::boundaryStruct boundaries;
    boundaries.North.setZero(imax);
    boundaries.West = Eigen::sin(grid.y);
    boundaries.South.setZero(imax);
    boundaries.East.setZero(jmax);

    // Declare problem matrices and vectors to solve: Au = b
    Eigen::SparseMatrix<f64> A(n, n); /**< Sparse weights matrix */
    EigenDefs::Vector<f64>   u(n);    /**< Solution vector */
    EigenDefs::Vector<f64>   b(n);    /**< Forcing vector */
    b.setZero();
    
    // Fill out sparse matrix using a list of triplets (i,j,value)
    // Internal
    std::vector<  Eigen::Triplet<f64>  > coefficients; /**< List of triplets to fill out sparse matrix with */
    for (u32 j=1; j<jmax-1; j++){
        for (u32 i=1; i<imax-1; i++){

            // Calculate grid spacing necessary from full grid
            f64 dx1 = grid.x[i]   - grid.x[i-1];
            f64 dx2 = grid.x[i+1] - grid.x[i];
            f64 dy1 = grid.y[j]   - grid.y[j-1];
            f64 dy2 = grid.y[j+1] - grid.y[j];

            // We consider BCs in the sparse matrix problem, fill general pattern in A matrix.
            const u32 idx = j*imax + i;
            u32 idx1;
            idx1 = (j-1)*imax + (i)  ; coefficients.push_back(  Eigen::Triplet<f64>(idx,idx1, -2./( dy1*(dy1+dy2) ))  );
            idx1 = (j)  *imax + (i-1); coefficients.push_back(  Eigen::Triplet<f64>(idx,idx1, -2./( dx1*(dx1+dx2) ))  );
            idx1 = (j)  *imax + (i)  ; coefficients.push_back(  Eigen::Triplet<f64>(idx,idx1,  2./(dx1*dx2)+2./(dy1*dy2))  );
            idx1 = (j)  *imax + (i+1); coefficients.push_back(  Eigen::Triplet<f64>(idx,idx1, -2./( dx2*(dx1+dx2) ))  );
            idx1 = (j+1)*imax + (i)  ; coefficients.push_back(  Eigen::Triplet<f64>(idx,idx1, -2./( dy2*(dy1+dy2) ))  );
            
            // Fill source term in b vector.
            b[idx] = valueSource(grid.x[i], grid.y[j]);
        }
    }
    // South Boundary
    u32 j=0;
    for (u32 i=1; i<imax-1; i++){
        const u32 idx = j*imax + i;
        coefficients.push_back(  Eigen::Triplet<f64>(idx,idx,1)  );
        b[idx] = boundaries.South[i];
        u[idx] = boundaries.South[i]; 
    }
    // North Boundary
    j=jmax-1;
    for (u32 i=1; i<imax-1; i++){
        const u32 idx = j*imax + i;
        coefficients.push_back(  Eigen::Triplet<f64>(idx,idx,1)  );
        b[idx] = boundaries.North[i];
        u[idx] = boundaries.North[i]; 
    }
    // West Boundary
    i64 i=0;
    for (u32 j=0; j<jmax; j++){
        const u32 idx = j*imax + i;
        coefficients.push_back(  Eigen::Triplet<f64>(idx,idx,1)  );
        b[idx] = boundaries.West[j];
        u[idx] = boundaries.West[j]; 
    }
    // East Boundary
    i=imax-1;
    for (u32 j=0; j<jmax; j++){
        const u32 idx = j*imax + i;
        coefficients.push_back(  Eigen::Triplet<f64>(idx,idx,1)  );
        b[idx] = boundaries.East[j];
        u[idx] = boundaries.East[j]; 
    }
    // Fill out sparse matrix
    A.setFromTriplets(coefficients.begin(), coefficients.end());

    INFO_MSG("Matrix-Vector setup finished");

    //## ================ ##//
    //## Solution Routine ##//
    //## ================ ##//
    // Diagonal Preconditioner
    // - see https://diamhomes.ewi.tudelft.nl/~mvangijzen/PhDCourse_DTU/LES5/TRANSPARANTEN/les5.pdf 
    // - see Section 4.1 https://homepage.tudelft.nl/d2b4e/burgers/lin_notes.pdf
    Eigen::SparseMatrix<f64> Mm1(n,n);          /**< Inversed Preconditioner M^-1*/
    Mm1 = A.diagonal().asDiagonal().inverse();

    // Preconditioned Conjugate Gradient (PCG)
    // - see Section 3.1 https://homepage.tudelft.nl/d2b4e/burgers/lin_notes.pdf
    // - see Section 4.1 https://homepage.tudelft.nl/d2b4e/burgers/lin_notes.pdf

    // Initialization
    u32 kappa = 0;          /**< iterate number*/
    u32 kappaMax = 5000;    /**< max iterate allowed */
    f64 tol = 1e-15;        /**< acceptable tolerance */
    f64 err = 1/0.0;        /**< residual error */
    EigenDefs::Vector<f64> rk(n), rkm1(n), rkm2(n); /**< residual vector */
    EigenDefs::Vector<f64> zk(n), zkm1(n), zkm2(n); /**< preconditioned residual vector */
    EigenDefs::Vector<f64> pk(n);                   /**< search/conjugate direction vector */
    EigenDefs::Vector<f64> Apk(n);                  /**< search/conjugate direction vector */
    f64 alphak, betak;                              /**< update coefficients */
    rk = b - A*u;

    do {

        // Calculate preconditioning residual vector
        zk = Mm1*rk;                            

        // Update kappa
        kappa++;
        rkm2 = rkm1; rkm1 = rk; 
        zkm2 = zkm1; zkm1 = zk;

        // Update of pk (search direction)
        if (kappa==1) pk = zk;
        else{
            betak = rkm1.dot(zkm1) / rkm2.dot(zkm2);
            pk    = zkm1 + betak*pk;
        }

        // Update iterate
        Apk = A*pk;
        alphak = rkm1.dot(zkm1) / pk.dot(Apk);
        u = u + alphak*pk;

        // Update residual
        rk = rkm1 - alphak*Apk;

        err = std::sqrt( rk.dot(rk)/rk.size() );
        FATAL_ITERATION(kappa, err);
        INFO_MSG("kappa = %-5u err = %1.4e", kappa, err);
    } while ((kappa < kappaMax) && (err > tol)); // Termination criteria
    

    // - see https://eigen.tuxfamily.org/dox/group__TutorialSparse.html
    // - see https://eigen.tuxfamily.org/dox/classEigen_1_1SparseLU.html
    // Eigen::SparseLU<Eigen::SparseMatrix<f64>> solver(A);  /**< LU factorization of A */
    // solver.analyzePattern(A); // Compute the column permutation to minimize the fill-in
    // solver.factorize(A);      // Compute the numerical factorization 
    // u = solver.solve(b);      // use the factorization to solve for the given right hand side

    //## =============== ##//
    //## Export solution ##//
    //## =============== ##//
    std::ofstream dataFile("data.bin", std::ios::out | std::ios::binary | std::ios::trunc); /**< Data output file, not using std::ios::app */ 
    dataFile.write(reinterpret_cast<const char*>(&imax), sizeof(imax));
    dataFile.write(reinterpret_cast<const char*>(&jmax), sizeof(jmax));

    u32 idx = 0;
    for (u32 j=0; j<jmax; j++){
        for (u32 i=0; i<imax; i++){
            // Plotting / postprocessing currently does not need to be done in such high precision
            f32 xPoint = (float) grid.x[i]; /**< f32 x-position */
            f32 yPoint = (float) grid.y[j]; /**< f32 y-position */
            f32 uPoint = (float) u[idx];    /**< f32 u-solution */

            dataFile.write(reinterpret_cast<const char*>(&xPoint), sizeof(xPoint));
            dataFile.write(reinterpret_cast<const char*>(&yPoint), sizeof(yPoint));
            dataFile.write(reinterpret_cast<const char*>(&uPoint), sizeof(uPoint));
            idx++;
        }
    }
    dataFile.close();

    INFO_MSG("Solution saved.");

    return EXIT_SUCCESS;
}
