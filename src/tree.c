#include "tree.h"

/*
 *    tree.c
 *    swot (Super W Of Theta)  mpi version
 *    Jean Coupon (2012-2016)
 */


/*
 *		Tree routines
 */


Tree buildTree(const Config *para, Point *data, Mask *mask, int dim, int firstCall){

   static long index, count;
   static Tree result;

   double d, dMax;
   long i, j, n, l, local_index, maxPoint;

   if(firstCall){

      /* number of nodes */
      result.size = countNodes(data->N, NLEAF);

      /* allocate memory slots (see structure description in main.h)*/
      result.left   = (long *)malloc(result.size*sizeof(long));
      result.right  = (long *)malloc(result.size*sizeof(long));
      result.N      = (double *)malloc(result.size*sizeof(double));
      result.r      = (double *)malloc(result.size*sizeof(double));
      result.w      = (unsigned char *)malloc(result.size*para->nsamples*sizeof(unsigned char));
      result.Ntot   = (double *)malloc(para->nsamples*sizeof(double));
      result.point  = createPoint(*para, result.size);
      if(para->corr != AUTO_3D && para->corr != CROSS_3D){
         result.cosx   =  (double *)malloc(2*result.size*sizeof(double));
         result.sinx   =  (double *)malloc(2*result.size*sizeof(double));
      }
      if(para->proj == COMO || para->proj == PHYS){
         result.distComo = (double *)malloc(result.size*sizeof(double));
      }

      count = index = ROOT;                                       /* index of the root (= 0) */
      for(i=0;i<result.size*para->nsamples;i++) result.w[i] = 0;  /* set weights to 0        */
      for(i=0;i<para->nsamples;i++) result.Ntot[i] = 0.0;         /* initialize Ntot         */

   }

   local_index = count++;                   /* index of THIS node             */
   //  result.N[local_index] = data->N; /* weighted number of points for THIS node */

   /* get mean properties of points inside the node <=> "weighted" center */
   getMeanPoint(*para, result.point, local_index, *data);

   /* total weigthed number of objects (N X mean weight)*/
   result.N[local_index] = (double)data->N*result.point.w[local_index];

   /* set weights from mask */
   int ndim;
   if(para->resample2D) ndim = 2;
   else ndim = NDIM;
   for(j=0;j<para->nsub;j++){  /* loop over subsamples */
      n = 0;
      for(i=0;i<ndim;i++){
         n += (mask->min[ndim*j + i] < result.point.x[NDIM*local_index + i] &&
            result.point.x[NDIM*local_index + i] < mask->max[ndim*j + i] );
         }
         if(n == ndim){  /* "result.point" is in the subsample "j" */
         for(i=0;i<para->nsamples;i++){  /* loop over resamplings  */
            result.w[para->nsamples*local_index + i] = mask->w[para->nsub*i+j];
         }
         break;
      }
   }

   /* pre-computed quantities */
   if(para->corr != AUTO_3D && para->corr != CROSS_3D){
      result.cosx[2*local_index+0] = cos(result.point.x[NDIM*local_index+0]*PI/180.0);
      result.cosx[2*local_index+1] = cos(result.point.x[NDIM*local_index+1]*PI/180.0);
      result.sinx[2*local_index+0] = sin(result.point.x[NDIM*local_index+0]*PI/180.0);
      result.sinx[2*local_index+1] = sin(result.point.x[NDIM*local_index+1]*PI/180.0);
   }
   if(para->proj == COMO || para->proj == PHYS){
      result.distComo[local_index] = distComo(result.point.x[NDIM*local_index+2], para->a);
   }

   /* compute node radius "r" for the open-angle approximation. r is defined as
   * the distance between the weighted center and the most distant point (angular
   * separation). For speed purpose, the test uses the cartesian approximation, but
   * the distance is then estimated accuratly according to the coordinate system.
   *
   * We make the assumption that box size estimated on the projected
   * coordinates should be a fair approximation
   * of the 3-D size.
   */

   /* test if deleting the Cart coordinates will slow down or not */

   dMax = 0.0; maxPoint = 0;
   for(n=0;n<data->N;n++){
      d = distAngPointCart(para, &(result.point), &local_index, data, &n);
      if(d > dMax){
         maxPoint = n;
         dMax     = d;
      }
   }

   if(para->corr != AUTO_3D && para->corr != CROSS_3D){
      result.r[local_index] = distAngPointSpher(para, &(result.point), &local_index, data, &maxPoint);
   }else{
      result.r[local_index] = d;
   }

   // DEBUGGING for wp(rp) testing
   // result.r[local_index] = d;

   /* if wp, use physical distance THIS DOES NOT SEEM TO WORK PROPERLY ???*/
   if(para->corr == AUTO_WP || para->corr == CROSS_WP ){
      //result.r[local_index]  = result.distComo[local_index]*result.r[local_index]*PI/180.0;
   }

   if(data->N > NLEAF){   /* node ----------------------------- */

      Point dataLeft, dataRight;

      /* split the node into two along the "dim" coordinate */
      splitData(*para, *data, dim, &dataLeft, &dataRight);

      /* next splitting coordinate */
      dim++; if(dim > NDIM-1) dim = 0;

      buildTree(para, &dataLeft, mask, dim, 0);    /* left child */
      result.left[local_index] = index;

      buildTree(para, &dataRight, mask, dim, 0);   /* right child */
      result.right[local_index] = index;

   }else{                 /* leaf ------------------------------ */
      result.left[local_index]  = -1;
      result.right[local_index] = -1;

      /* keep track of the total number of objects in the tree per sample */
      for(i=0;i<para->nsamples;i++){
         result.Ntot[i] += result.point.w[local_index]*(double)NLEAF*result.w[para->nsamples*local_index + i];
      }
   }

   /* return the index of this node (index is static) */
   index = local_index;

   return result;
}

void resample(const Config *para, const Point *data, int dim, Mask *mask, int firstCall){
   /* splits data into a number of sub samples, resample it and builds up a
   * mask with weights. Then send the mask (if MASTER)
   * or receive it (if SLAVE). The mask is used in buildTree()
   * to attribute a weight to each node. */

   static long depth, depthSample, count;
   long i, j, l, rank;

   int ndim;
   if(para->resample2D) ndim = 2;
   else ndim = NDIM;

   if(firstCall){

      depthSample = 0;
      depth       = 0; /* depth of the node. Number of parallel nodes is 2^depth */
      count       = 0; /* counter for mask array                                 */

      /* check up sample values value and evaluate depthSample */
      if(para->nsamples == 0){
         return;
         /*
      }else if(para->nsamples > 256 || para->nsub > 256){
      if(para->verbose)
      fprintf(stderr,"\n%s: **ERROR** nsamplesand nsub must be <= 256. Exiting...\n", MYNAME);
      MPI_Finalize();
      exit(EXIT_FAILURE);
      */
   }else if(lowestPowerOfTwo(para->nsub, &depthSample) != para->nsub){
      if(para->verbose)
      fprintf(stderr,"\n%s: **ERROR** nsub must be a power of 2. Exiting...\n", MYNAME);
      MPI_Finalize();
      exit(EXIT_FAILURE);
   }

   /* initialisation */
   mask->min = (double *)malloc(ndim*para->nsub*sizeof(double));
   mask->max = (double *)malloc(ndim*para->nsub*sizeof(double));
   mask->w   = (unsigned char *)malloc(para->nsamples*para->nsub*sizeof(unsigned char));

   if(para->rank == MASTER){

      if(para->nsub > data->N){
         if(para->verbose)
         fprintf(stderr,"\n%s: **ERROR** nsub must be < Npoints. Exiting...\n", MYNAME);
         exit(EXIT_FAILURE);
      }

      for(i=0;i<para->nsamples*para->nsub;i++) mask->w[i] = 0;

      for(i=0;i<para->nsamples;i++){
         for(j=0;j<para->nsub;j++){
            /* i: resample draw index
            * j: subsample index
            */
            switch(para->err){
               case SUBSAMPLE: mask->w[para->nsub*i + j] = (i == j);             break;
               case JACKKNIFE: mask->w[para->nsub*i + j] = !(i == j);            break;
               case BOOTSTRAP: mask->w[para->nsub*i + randInt(para->nsub)] += 1; break;
            }
         }
      }

      /* make sure no mask value is larger than 256 */
      for(i=0;i<para->nsamples*para->nsub;i++){
         if(mask->w[i]  > 255){
            if(para->verbose)
            fprintf(stderr,"\n%s: **ERROR** a mask value > 255 was produced, stack overflow will occur. Exiting...\n", MYNAME);
            exit(EXIT_FAILURE);
         }
      }

   }
}

if(para->rank == MASTER){
   /* one level down */
   depth++;

   if(depth <= depthSample){

      Point dataLeft, dataRight;

      /* split the node into two along the "dim" coordinate */
      splitData(*para, *data, dim, &dataLeft, &dataRight);

      /* next splitting coordinate */
      dim++; if(dim > ndim-1) dim = 0;

      resample(para, &dataLeft,  dim, mask, 0);
      resample(para, &dataRight, dim, mask, 0);

   }else{

      /* compute limits of the mask */
      for(dim=0;dim<ndim;dim++){
         mask->min[ndim*count + dim] = data->x[NDIM*0+dim];
         mask->max[ndim*count + dim] = data->x[NDIM*0+dim];
      }
      for(i=1;i<data->N;i++){
         for(dim=0;dim<ndim;dim++){
            mask->min[ndim*count + dim] = MIN(mask->min[ndim*count + dim], data->x[NDIM*i+dim]);
            mask->max[ndim*count + dim] = MAX(mask->max[ndim*count + dim], data->x[NDIM*i+dim]);
         }
      }
      count++;
   }

   /* one level up */
   depth--;
}

if(firstCall){
   MPI_Bcast(mask->min, ndim*para->nsub, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
   MPI_Bcast(mask->max, ndim*para->nsub, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
   MPI_Bcast(mask->w,   para->nsamples*para->nsub, MPI_UNSIGNED_CHAR, MASTER, MPI_COMM_WORLD);
}

return;
}

void freeMask(const Config para, Mask mask){

   if(para.nsamples > 0){
      free(mask.min);
      free(mask.max);
      free(mask.w);
   }

}

void printTree(const Config para, char *fileOutName, const Tree tree, long i, long NLeaf, int firstCall){
   static FILE *fileOut;
   int l, dim;

   if(firstCall) fileOut = fopenAndCheck(fileOutName,"w", para.verbose);
   //if(firstCall) fileOut = stdout;

   if(tree.N[i] > NLeaf){
      printTree(para, fileOutName, tree, tree.left[i], NLeaf, 0);
      printTree(para, fileOutName, tree, tree.right[i], NLeaf, 0);
   }else{
      for(dim=0;dim<NDIM;dim++)   fprintf(fileOut,"%f ", tree.point.x[NDIM*i+dim]);
      for(l=0;l<para.nsamples;l++)  fprintf(fileOut,"%d ", tree.w[para.nsamples*i + l] );
      fprintf(fileOut,"     %d \n", para.rank);
   }

   if(firstCall) fclose(fileOut);

   //if(firstCall)  MPI_Barrier(MPI_COMM_WORLD);
}

void freeTree(const Config para, Tree tree){
   /* Frees the tree "tree". */

   if(para.corr != AUTO_3D && para.corr != CROSS_3D){
      free(tree.cosx);
      free(tree.sinx);
   }
   if(para.proj == COMO || para.proj == PHYS){
      free(tree.distComo);
   }

   free(tree.left);
   free(tree.right);
   free(tree.N);
   free(tree.r);
   free(tree.w);
   free(tree.Ntot);
   freePoint(para, tree.point);

   return;
}

long splitTree(const Config *para, const Tree *tree1, const long root, const long Ncpu, int firstCall){
   /* returns the id of the node to be computed by rank n. */

   static long count, rootRank;

   if(firstCall){
      count = 0;
      rootRank = -1;
   }

   if(Ncpu > 1 && tree1->left[root] != -1){
      switch(PARITY(Ncpu)){
         case EVEN:
         splitTree(para, tree1, tree1->left[root],  Ncpu/2, 0);
         splitTree(para, tree1, tree1->right[root], Ncpu/2, 0);
         break;

         case ODD:
         splitTree(para, tree1, tree1->left[root],  (Ncpu+1)/2, 0);
         splitTree(para, tree1, tree1->right[root], (Ncpu-1)/2, 0);
         break;
      }
   }else{
      if(count++ == para->rank) rootRank = root;
   }

   return rootRank;
}

long countNodes(long N, long NLeaf){
   /* Simply counts the number of nodes in a tree
   * from a data set of N points. NLeaf is the
   * maximum number of objects in a leaf.
   */

   long result = 0;

   if(N > NLeaf){
      switch(PARITY(N)){
         case EVEN:
         result += 2*countNodes(N/2, NLeaf);
         break;
         case ODD:
         result += countNodes((N+1)/2, NLeaf);
         result += countNodes((N-1)/2, NLeaf);
         break;
      }
   }else{
      return 1;
   }
   return result + 1;
}