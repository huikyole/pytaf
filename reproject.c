/**
 * reproject.c
 * Authors: Yizhao Gao <ygao29@illinois.edu>
 * Date: {12/06/2017}
 * Muqun Yang<myang6@hdfgroup.org> (adapted for python wrapper)
 * Date: {05/21/2019}
 */


#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <omp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * struct LonBlocks: data structure for grid-based spatial indexing. Each "struct LonBlocks" holds one row (latitude) of the spatial indexing
 * ITEMS:
 * 	double blockSizeR:	the longtitude range of each block
 * 	int nBlocks:		the number of longtitude blocks in this row
 *	int * indexID:		the starting and ending index (of the long arrays of data) of locations in each block
 */
struct LonBlocks {
	double blockSizeR;
	int nBlocks;
	int * indexID;
};

struct LonBlocks * pointIndexOnLatLon(double ** plat, double ** plon, double*newLat, double*newLon,int * oriID, int count, int nBlockY, double maxradian) {

	double *lat = *plat;
	double *lon = *plon;

	struct LonBlocks *blockIndex = NULL;
	int ** pointsInB = NULL;
	if(NULL == (blockIndex = (struct LonBlocks *)malloc(sizeof(struct LonBlocks) * nBlockY))) {
		printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
		exit(1);	
	}
	if(NULL == (pointsInB = (int **)malloc(sizeof(int *) * nBlockY))) {
		printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
		exit(1);	
	}

	double latBlockR = M_PI/nBlockY;

	blockIndex[0].blockSizeR = 2 * M_PI;
	blockIndex[0].nBlocks = 1;
	if(NULL == (blockIndex[0].indexID = (int *) malloc (sizeof(int) * 2))) {
		printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
		exit(1);	
	}
	if(NULL == (pointsInB[0] = (int *)malloc(sizeof(int)))) {
		printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
		exit(1);	
	}
	pointsInB[0][0] = 0;

	blockIndex[nBlockY - 1].blockSizeR = 2 * M_PI;
	blockIndex[nBlockY - 1].nBlocks = 1;
	if(NULL == (blockIndex[nBlockY - 1].indexID = (int *) malloc (sizeof(int) * 2))) {
		printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
		exit(1);	
	}
	if(NULL == (pointsInB[nBlockY - 1] = (int *)malloc(sizeof(int)))) {
		printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
		exit(1);	
	}
	pointsInB[nBlockY - 1][0] = 0;


	int i, j;
	double highestLat;
	for(i = 1; i < nBlockY - 1; i++) {

		if(i < (nBlockY + 1) / 2) {
			highestLat = -M_PI/2 + latBlockR * i;
		}
		else {
			highestLat = -M_PI/2 + latBlockR * (i + 1);
		}
		blockIndex[i].nBlocks = (int)(2 * M_PI * cos(highestLat)/ maxradian);
		if(blockIndex[i].nBlocks < 4) {
			blockIndex[i].nBlocks = 1;
		}
		blockIndex[i].blockSizeR = 2 * M_PI / blockIndex[i].nBlocks;

		if(NULL == (blockIndex[i].indexID = (int *) malloc (sizeof(int) * (blockIndex[i].nBlocks + 1)))) {
			printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
			exit(1);
        	}
		if(NULL == (pointsInB[i] = (int *) malloc (sizeof(int) * (blockIndex[i].nBlocks + 1)))) {
			printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
			exit(1);
        	}
		for(j = 0; j < blockIndex[i].nBlocks; j++) {
			pointsInB[i][j] = 0; 
		}
		
	}

	int rowID, colID;
	for(i = 0; i < count; i++) {
	
		rowID = (int)((lat[i] + M_PI/2) / latBlockR);
		
		if(rowID >= 0 && rowID < nBlockY) {
			colID = (int)((lon[i] + M_PI) / blockIndex[rowID].blockSizeR);
			if(colID >= 0 && colID < blockIndex[rowID].nBlocks) {
				pointsInB[rowID][colID] ++;
			}
		}
	}

	int newCount = 0;

	for(i = 0; i < nBlockY; i++) {
		blockIndex[i].indexID[0] = newCount;
		for(j = 0; j < blockIndex[i].nBlocks; j++)	{
			newCount += pointsInB[i][j];
			blockIndex[i].indexID[j+1] = newCount;
			pointsInB[i][j] = blockIndex[i].indexID[j];
		}
	}

	for(i = 0; i < count; i++) {
	
		rowID = (int)((lat[i] + M_PI/2) / latBlockR);
		
		if(rowID >= 0 && rowID < nBlockY) {
			colID = (int)((lon[i] + M_PI) / blockIndex[rowID].blockSizeR);
			if(colID >= 0 && colID < blockIndex[rowID].nBlocks) {
				
				newLon[pointsInB[rowID][colID]] = lon[i];
				newLat[pointsInB[rowID][colID]] = lat[i];
				oriID[pointsInB[rowID][colID]] = i;
				pointsInB[rowID][colID] ++;
			}
		}
	}

	for(i = 0; i < nBlockY; i++) {
            if(pointsInB[i] != NULL)
                free(pointsInB[i]);
	}


        if(pointsInB != NULL)
            free(pointsInB);
	return blockIndex;
}

 /**
 * NAME:	nearestNeighborBlockIndex
 * DESCRIPTION:	Find the nearest neighboring source cell's ID for each target cell
 * PARAMETERS:
 *	double ** psouLat:	the pointer to the array of latitudes of source cells (the data are changed during in the function, so please do the output before this function)
 *	double ** psouLon:	the pointer to the array of longitudes of source cells (the data are changed during in the function, so please do the output before this function)
 *	int nSou:		the number of source cells
 *	double * tarLat:	the latitudes of target cells
 *	double * tarLon:	the longitudes of target cells
 *	int * tarNNSouID:	the output IDs of nearest neighboring source cells 
 *	double * tarNNDis	the output nearest distance for each target cell (input NULL if you don't need this field)
 *	int nTar:		the number of target cells
 *	double maxR:		the maximum distance (in meters) to define neighboring cells
 * Output: 	
 *	int * tarNNSouID:	the output IDs of nearest neighboring source cells 
 *	double * tarNNDis	the output nearest distance for each target cell (input NULL if you don't need this field)
 */
void nearestNeighborBlockIndex(double ** psouLat, double ** psouLon, int nSou, double * tarLat, double * tarLon, int * tarNNSouID, double * tarNNDis, int nTar, double maxR) {

	double * souLat = *psouLat;
	double * souLon = *psouLon;

	//const double earthRadius = 6367444;
	const double earthRadius = 6371009;
	double maxradian = maxR / earthRadius;

	double blockSizeRadian = maxradian;
	if(maxR < 1000) {
		blockSizeRadian = 1000 / earthRadius;
	}

	int nBlockY = M_PI / blockSizeRadian;

	double latBlockR = M_PI / nBlockY;

	int i, j, k, kk, l;
#pragma omp parallel for
	for(i = 0; i < nSou; i++) {
		
/*		if(i == 0)
		{
			printf("%d threads\n", omp_get_num_threads());
		}
*/
		souLat[i] = souLat[i] * M_PI / 180;
		souLon[i] = souLon[i] * M_PI / 180;
	}

#pragma omp parallel for
	for(i = 0; i < nTar; i++) {
		tarLat[i] = tarLat[i] * M_PI / 180;
		tarLon[i] = tarLon[i] * M_PI / 180;
	}

	int * souID;
	if(NULL == (souID = (int *)malloc(sizeof(double) * nSou))) {
		printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
		exit(1);
	}

	double * newLat = NULL;
	double * newLon = NULL;
	if(NULL == (newLon = (double *)malloc(sizeof(double) * nSou))) {
		printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
		exit(1);
	} 
	if(NULL == (newLat = (double *)malloc(sizeof(double) * nSou))) {
		printf("ERROR: Out of memory at line %d in file %s\n", __LINE__, __FILE__);
		exit(1);
	}

	struct LonBlocks * souIndex = pointIndexOnLatLon(psouLat, psouLon, newLat,newLon,souID, nSou, nBlockY, blockSizeRadian);


#pragma omp parallel for private(j, k, kk, l)
	for(i = 0; i < nTar; i ++) {
		
/*		if(i == 0)
		{
			printf("%d threads\n", omp_get_num_threads());
		}
*/
		double tLat = tarLat[i];
		double tLon = tarLon[i];
		double sLat, sLon;
		int rowID, colID;
		double pDis;
		double nnDis;
		int nnSouID= -1;
/*
		if(i == 0) {
			printf("%d\n", omp_get_num_threads());
		}
*/
		rowID = (tLat + M_PI / 2) / latBlockR;

		nnDis = -1;

		for(j = rowID - 1; j < rowID + 2; j ++) {
			if(j < 0 || j >= nBlockY) {
				continue;
			}
			colID = (tLon + M_PI) / souIndex[j].blockSizeR;

			if(souIndex[j].nBlocks == 1) {
				for(l = souIndex[j].indexID[0]; l < souIndex[j].indexID[1]; l++) {
					
					sLat = newLat[l];
					sLon = newLon[l];

					pDis = acos(sin(tLat) * sin(sLat) + cos(tLat) * cos(sLat) * cos(tLon - sLon));

					if((nnDis < 0 || nnDis > pDis) && pDis <= maxradian) {

						nnDis = pDis;
						nnSouID = souID[l];
					}
				}
			} 
			else {
				for(k = colID - 1; k < colID + 2; k ++) {
					kk = k;
					if(kk < 0) {
						kk = souIndex[j].nBlocks-1; 
					}
					if(kk >= souIndex[j].nBlocks) {
						kk = 0;
					}
					for(l = souIndex[j].indexID[kk]; l < souIndex[j].indexID[kk+1]; l++) {
						
						sLat = newLat[l];
						sLon = newLon[l];

						pDis = acos(sin(tLat) * sin(sLat) + cos(tLat) * cos(sLat) * cos(tLon - sLon));

						if((nnDis < 0 || nnDis > pDis) && pDis <= maxradian) {

							nnDis = pDis;
							nnSouID = souID[l];
						}
				
					}
				}
			}
		}


		if(nnDis < 0) {
			tarNNSouID[i] = -1;
			if(tarNNDis != NULL) { 
				tarNNDis[i] = -1;
			}
		}
		else {
			tarNNSouID[i] = nnSouID;
			if(tarNNDis != NULL) {
				tarNNDis[i] = nnDis * earthRadius;
			}
		}
			
	}

	if (souID != NULL)
		free(souID);
	for(i = 0; i < nBlockY; i++) {
            // printf("%d,\t%lf\n", souIndex[i].nBlocks, souIndex[i].blockSizeR);
		if (souIndex[i].indexID != NULL)
			free(souIndex[i].indexID);
	}
	if (souIndex != NULL)
		free(souIndex);
	free(newLat);
	free(newLon);
	return; 
}

/**
 * NAME:	nnInterpolate
 * DESCRIPTION:	Nearest neighbor interpolation
 * PARAMETERS:
 * 	double * souVal:	the input values at source cells
 * 	double * tarVal:	the output values at target cells
 * 	int * tarNNSouID:	the IDs of nearest neighboring source cells for each target cells (generated from "nearestNeighbor") 
 *	int nTar:		the number of target cells
 * Output: 	
 * 	double * tarVal:	the output values at target cells
 */ 
void nnInterpolate(double * souVal, double * tarVal, int * tarNNSouID, int nTar) {

	int nnSouID;
	int i;

#pragma omp parallel for private(nnSouID)
	for(i = 0; i < nTar; i++) {
		nnSouID = tarNNSouID[i];
		if(nnSouID < 0) {
			tarVal[i] = -999;
		}
		else {
			tarVal[i] = souVal[nnSouID];
		}
	}
}


/**
 * NAME:	summaryInterpolate
 * DESCRIPTION:	Interpolation (summary) from fine resolution to coarse resolution
 * PARAMETERS:
 * 	double * souVal:	the input values at source cells
 * 	int * souNNTarID:	the IDs of nearest neighboring target cells for each source cells (generated from "nearestNeighbor")
 * 	int nSou:		the number of source cells
 * 	double * tarVal:	the output (average) values at target cells
 * 	double * tarSD:		the standard deviation (SD) value at target cells (can be NULL if no SD values need to be reported)
 * 	int * nSouPixels:	the output numbers of contributing source cells to each target cell
 *	int nTar:		the number of target cells
 * Output:
 * 	double * tarVal:	the output (average) values at target cells
 * 	double * tarSD:		the standard deviation (SD) value at target cells (can be NULL if no SD values need to be reported)
 * 	int * nSouPixels:	the output numbers of contributing source cells to each target cell
 */
void summaryInterpolate(double * souVal, int * souNNTarID, int nSou, double * tarVal, double * tarSD, int * nSouPixels, int nTar) {

	int i;
	for(i = 0; i < nTar; i++) {
	
		tarVal[i] = 0;
		if (tarSD != NULL) {
			tarSD[i] = 0;
		}
		nSouPixels[i] = 0;
	}

	int nnTarID;
	for(i = 0; i < nSou; i++) {
		
		nnTarID = souNNTarID[i];
		if(nnTarID >= 0 && souVal[i] >= 0) {
			tarVal[nnTarID] += souVal[i];
			if (tarSD != NULL) {
				tarSD[nnTarID] += souVal[i] * souVal[i];
			}
			nSouPixels[nnTarID] ++;	
		}
	}


	for(i = 0; i < nTar; i++) {
	
		if(nSouPixels[i] > 0) {
			tarVal[i] = tarVal[i] / nSouPixels[i];
			if (tarSD != NULL) {
				if(tarSD[i] / nSouPixels[i] - tarVal[i] * tarVal[i] < 0) {
					tarSD[i] = 0;
				}
				else {
					tarSD[i] = sqrt(tarSD[i] / nSouPixels[i] - tarVal[i] * tarVal[i]);
				}
			}
			
		}
		else {
			tarVal[i] = -999;
			if (tarSD != NULL) {
				tarSD[i] = -999;
			}
		}
	}
}



/**
 * NAME:	clipping
 * DESCRIPTION:	Clip output radiance values based on mask
 * PARAMETERS:
 *	double * val: 		the output radicance values to be clipped; radiance values will be set to -999 if the mask value is also -999
 *	double * mask:		the mask for cliping, could be the radiance value after resampling
 *	int nPixels:		the number of pixels for both val and mask
 */
void clipping(double * val, double * mask, int nPixels)
{
	int i = 0;
	for(i = 0; i < nPixels; i++)
	{
		if(mask[i] == -999)
		{
			val[i] = -999;
		}
	}
}


