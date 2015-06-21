/*-------------------------------------------------------------------------*
 *swot (Super W Of Theta)  mpi version                                     *
 *Jean Coupon, Alexie Leauthaud (2012)                                     *
 *-------------------------------------------------------------------------*

Feb. 8th, 2013

Program to compute two-point correlation functions based on 
"divide and conquer" algorithms, mainly, but not limited to:
- data storage in a binary tree, 
- approximation at large scale,
- parellelization.
Supports auto and cross correlations and galaxy-galaxy lensing

Contributions:
- The algorithm to compute the number of pairs from a tree is 
based on Martin Kilbinger's Ahtena code:
http://www2.iap.fr/users/kilbinge/athena/
- The gal-gal lensing algorithm is based on
Alexie Leauthaud's code
(see  Leauthaud et al. (2010),  2010ApJ...709...97L).

Usage:  mpirun -np [Ncpu] swot -c configFile [options]: run the program
        swot -d: display a default configuration file

Important: if using "RADEC" coordinates, the angle
in the input catalogues must be in decimal degrees.

-------------------------------------------------------------------------
swot (Super W Of Theta)  mpi version                                     
-------------------------------------------------------------------------

* w(theta):
swot -c test/auto.para [-corr cross]

* Gal-gal lensing
swot -c test/gglens.para

-------------------------------------------------------------------------
Options
-------------------------------------------------------------------------

* Estimator
The choice for the estimator can be made among:
ls: Landy & Szalay (1993) estimator (default)
nat: Natural estimator: 1+DD/RR
ham: Hamilton (1993) estimator
peebles: Peebles (1993) estimator

* Open-angle (OA). From Athena's documentation:
"If two nodes see each other under angles which are smaller than the open-angle
threshold (OATH in config file), the tree is not further descended and those
nodes are correlated directly. The mean, weighted galaxy ellipticities of the
both nodes are multiplied, the angular separation is the binned barycenter
distance.  This is of course very time-saving, which is the great advantage of a
tree code over a brute-force approach. It introduces however errors in the
correlation function, in particular on large scales, because the galaxy
ellipticities are smeared out. One should play around with the value of OATH to
see its influence on the results. As a guideline, OATH = 0.05 (about 3 degrees)
can be used."

* default input format for "number" [-weighted]
RA DEC X [weight]
change column ids with: -cols1 1,2,3[,4]

* default input format for "auto" and "cross" [-proj como/phys] [-weighted]
RA DEC [z] [weight]
This means -> RA DEC z (if -proj phys), RA DEC w (if -weighted), 
or  RA DEC z w (if -proj phys -weighted) 
change column ids with: -cols1 1,2[,3][,4]

* default input format for "auto_3D" and "cross_3D" [-weighted]
X Y Z [weight]
change column ids with: -cols1 1,2,3[,4]
Trick: for 2D cartesian correlations, simply fix the 3rd column
to a constant number

* default input format for "auto_wp" and "cross_wp"
RA DEC z [weight]
change column ids with: -cols1 1,2,3[,4]

* default input format for "gglens"
lenses (-cols1) RA DEC z sigz
change column ids with: -cols1 1,2,3,4
sources (-cols2) RA DEC z sigz e1 e2 weight
change column ids with: -cols2 1,2,3,4,5,6,7

* default input format for "gglens" to compute calibration factor (-calib yes)
lenses (-cols1) RA DEC z sigz
change column ids with: -cols1 1,2,3,4
sources (-cols2) RA DEC z sigz calib e2 weight
change column ids with: -cols2 1,2,3,4,5,6,7
Where calib = 1+m or c

-------------------------------------------------------------------------
Compilation
-------------------------------------------------------------------------

To compile it, you need to have mpicc installed on your 
machine. Please visit http://www.open-mpi.org/ for more 
information. You also need the gsl library
(http://www.gnu.org/software/gsl/). Then run:
> make
If you want to use a different compiler than gcc (for example icc),
you need first to build mpicc with the following command:
(from intel website)
> ./configure --prefix=/usr/local CC=icc CXX=icpc F77=ifort FC=ifort
> make all install

-------------------------------------------------------------------------
Memory usage
-------------------------------------------------------------------------

To reduce the memory usage one can reduce the number of resamplings samples 
(-nsamples 8) for example, but the covariance matrix is likely to be 
inaccurate

Maximum efficiency is reached when the number of cpus is a power of two.

