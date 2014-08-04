/******************************************************************************
*       SOFA, Simulation Open-Framework Architecture, version 1.0 beta 4      *
*                (c) 2006-2009 MGH, INRIA, USTL, UJF, CNRS                    *
*                                                                             *
* This library is free software; you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as published by    *
* the Free Software Foundation; either version 2.1 of the License, or (at     *
* your option) any later version.                                             *
*                                                                             *
* This library is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License *
* for more details.                                                           *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this library; if not, write to the Free Software Foundation,     *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.          *
*******************************************************************************
*                               SOFA :: Modules                               *
*                                                                             *
* Authors: The SOFA Team and external contributors (see Authors.txt)          *
*                                                                             *
* Contact information: contact@sofa-framework.org                             *
******************************************************************************/

#ifndef IMAGE_IMAGEALGORITHMS_H
#define IMAGE_IMAGEALGORITHMS_H

#include "ImageTypes.h"
#include <sofa/defaulttype/Vec.h>
#include <sofa/helper/rmath.h>
#include <sofa/defaulttype/Mat.h>
#include <set>
#include <vector>
#include <array>

#ifdef USING_OMP_PRAGMAS
#include <omp.h>
#endif

using namespace cimg_library;

/**
*  Move points to the centroid of their voronoi region
*  returns true if points have moved
*/

template<typename real>
bool Lloyd (std::vector<sofa::defaulttype::Vec<3,real> >& pos,const std::vector<unsigned int>& voronoiIndex, CImg<unsigned int>& voronoi)
{
    typedef sofa::defaulttype::Vec<3,real> Coord;
    unsigned int nbp=pos.size();
    bool moved=false;

#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
    for (unsigned int i=0; i<nbp; i++)
    {
        // compute centroid
        Coord C,p;
        unsigned int count=0;
        bool valid=true;

        cimg_forXYZ(voronoi,x,y,z) if (voronoi(x,y,z)==voronoiIndex[i])
        {
            C+=Coord(x,y,z);
            count++;
        }
        if(!count) goto stop;
        C/=(real)count;

        // check validity
        for (unsigned int j=0; j<3; j++) p[j]=sofa::helper::round(C[j]);
        if (voronoi(p[0],p[1],p[2])!=voronoiIndex[i]) valid=false; // out of voronoi
        else { for (unsigned int j=0; j<nbp; j++) if(i!=j) if(sofa::helper::round(pos[j][0])==p[0]) if(sofa::helper::round(pos[j][1])==p[1]) if(sofa::helper::round(pos[j][2])==p[2]) valid=false; } // check occupancy

        while(!valid)  // get closest unoccupied point in voronoi
        {
            real dmin=cimg_library::cimg::type<real>::max();
            cimg_forXYZ(voronoi,x,y,z) if (voronoi(x,y,z)==voronoiIndex[i])
            {
                real d2=(C-Coord(x,y,z)).norm2();
                if(dmin>d2) { dmin=d2; p=Coord(x,y,z); }
            }
            if(dmin==cimg_library::cimg::type<real>::max()) goto stop;// no point found
            bool val2=true; for (unsigned int j=0; j<nbp; j++) if(i!=j) if(sofa::helper::round(pos[j][0])==p[0]) if(sofa::helper::round(pos[j][1])==p[1]) if(sofa::helper::round(pos[j][2])==p[2]) val2=false; // check occupancy
            if(val2) valid=true;
            else voronoi(p[0],p[1],p[2])=0;
        }

        if(pos[i][0]!=p[0] || pos[i][1]!=p[1] || pos[i][2]!=p[2]) // set new position if different
        {
            pos[i] = p;
            moved=true;
        }
stop: ;
    }

    return moved;
}


/**
* solve the local eikonal system:  || grad d || = 1/b given the 6 neihborood values of d at [-dx,+dx,-dy,+dy,-dz,+dz] where [dx,dy,dz] is the voxel size
* using upwind first order approximation (http://math.berkeley.edu/~sethian/2006/Publications/Book/2006/)
* if(d<0) these values are not used (untreated voxels in the fast marching algorithm)
* if values at [-2dx,+2dx,-2dy,+2dy,-2dz,+2dz] (@param d2) are provided (>0), use more accurate second order approximation (cf "A second-order fast marching eikonal solver", James Rickett and Sergey Fomel , 1999)
*/
template<typename real>
real Eikonal(const sofa::defaulttype::Vec<6,real>& d,const sofa::defaulttype::Vec<6,real>& d2,const sofa::defaulttype::Vec<3,real>& voxelsize, const real b=(real)1.0)
{
    // get minimum distance in each direction and some precomputations
    unsigned int nbValid=3;
    sofa::defaulttype::Vec<3,real> D(-1,-1,-1),D2,S;
    real B2=(real)1./(b*b),val;
    for (unsigned int i=0; i<3; i++)
    {
        if(d[2*i]>=0 && d2[2*i]>=0)  { val=(4.0*d[2*i]-d2[2*i])/3.0;  if(val<D[i] || D[i]==-1) { D[i]=val; S[i]=9.0/(4.0*voxelsize[i]*voxelsize[i]); } }
        else if(d[2*i+1]>=0 && d2[2*i+1]>=0)  { val=(4.0*d[2*i+1]-d2[2*i+1])/3.0;  if(val<D[i] || D[i]==-1) { D[i]=val; S[i]=9.0/(4.0*voxelsize[i]*voxelsize[i]); } }
        else if(d[2*i]>=0)  { val=d[2*i];  if(val<D[i] || D[i]==-1) { D[i]=val;  S[i]=1.0/(voxelsize[i]*voxelsize[i]); } }
        else if(d[2*i+1]>=0)  { val=d[2*i+1];  if(val<D[i] || D[i]==-1) { D[i]=val;  S[i]=1.0/(voxelsize[i]*voxelsize[i]); } }
        else nbValid--;
        D2[i]=D[i]*D[i];
    }
    // solve sum S_i*(U-D_i)^2  = 1/b^2
    while(1)
    {
        if(nbValid==0) return -1; // no valid neighbor
        else if(nbValid==1) { for (unsigned int i=0; i<3; i++) if(D[i]>=0.) return (D[i]+voxelsize[i]/b); } // one valid neighbor -> simple 1D propagation
        else // two or three valid neighbors -> quadratic equation
        {
            real A=(real)0,B=(real)0,C=-B2;
            for (unsigned int i=0; i<3; i++) if(D[i]>=0.) { A+=S[i]; B+=D[i]*S[i];  C+=D2[i]*S[i]; } B*=(real)-2.;
            real Delta=B*B-4.0*A*C;
            if(Delta<0) { if(D[0]>D[1]) { if(D[0]>D[2]) D[0]=-1; else D[2]=-1; } else { if(D[1]>D[2]) D[1]=-1; else D[2]=-1; }  nbValid--; }
            else
            {
                real U=0.5*(sqrt(Delta)-B)/A;  // largest root since A>0
                if(U>D[0]) if(U>D[1]) if(U>D[2]) return U;
                // one entry should be canceled
                if(D[0]>D[1]) { if(D[0]>D[2]) D[0]=-1; else D[2]=-1; }
                else { if(D[1]>D[2]) D[1]=-1; else D[2]=-1; }  nbValid--;
            }
        }
    }
    return -1;
}

/**
* Update geodesic distances in the image, given a bias distance function b(x).
* This is equivalent to solve for the eikonal equation || grad d(x) || = 1/b(x) with d(p)=0 at @param pos
* using fast marching method presented from sethian http://math.berkeley.edu/~sethian/2006/Publications/Book/2006/
* distances should be intialized (<0 outside the object, >=0 inside, and = 0 for seeds)
* returns @param voronoi and @param distances
*/


template<typename real,typename T>
void fastMarching (std::set<std::pair<real,sofa::defaulttype::Vec<3,int> > > &trial,CImg<real>& distances, CImg<unsigned int>& voronoi, const sofa::defaulttype::Vec<3,real>& voxelsize, const CImg<T>* biasFactor=NULL)
{
    typedef sofa::defaulttype::Vec<3,int> iCoord;
    typedef sofa::defaulttype::Vec<6,real> Dist;
    typedef std::pair<real,iCoord > DistanceToPoint;
    const iCoord dim(distances.width(),distances.height(),distances.depth());

    // init
    sofa::defaulttype::Vec<6,  iCoord > offset; // image coord offsets related to 6 neighbors
    for (unsigned int i=0; i<3; i++) { offset[2*i][i]=-1; offset[2*i+1][i]=1;}
    unsigned int nbOffset=offset.size();
    CImg<bool> alive(dim[0],dim[1],dim[2]); alive.fill(false);

    // FMM
    while( !trial.empty() )
    {
        DistanceToPoint top = *trial.begin();
        trial.erase(trial.begin());
        iCoord v = top.second;
        alive(v[0],v[1],v[2])=true;

        unsigned int vor = voronoi(v[0],v[1],v[2]);
        real b1; if(biasFactor) b1=(real)(*biasFactor)(v[0],v[1],v[2]); else  b1=1.0;

        // update neighbors
        for (unsigned int i=0; i<nbOffset; i++)
        {
            // update distance on neighbors using their own neighbors
            iCoord v2 = v + offset[i];
            if(v2[0]>=0) if(v2[1]>=0) if(v2[2]>=0) if(v2[0]<dim[0]) if(v2[1]<dim[1]) if(v2[2]<dim[2])
                if(!alive(v2[0],v2[1],v2[2]))
                {
                    // get neighboring alive values
                    iCoord v3=v2;
                    Dist d,d2;
                    for (unsigned int j=0; j<3; j++)
                    {
                        v3[j]--;  if(v3[j]>=0 && alive(v3[0],v3[1],v3[2])) d[2*j]= distances(v3[0],v3[1],v3[2]); else d[2*j]=-1;
                        v3[j]--;  if(v3[j]>=0 && alive(v3[0],v3[1],v3[2])) d2[2*j]= distances(v3[0],v3[1],v3[2]); else d2[2*j]=-1;
                        v3[j]+=3; if(v3[j]<dim[j] && alive(v3[0],v3[1],v3[2])) d[2*j+1]= distances(v3[0],v3[1],v3[2]); else d[2*j+1]=-1;
                        v3[j]++;  if(v3[j]<dim[j] && alive(v3[0],v3[1],v3[2])) d2[2*j+1]= distances(v3[0],v3[1],v3[2]); else d2[2*j+1]=-1;
                        v3[j]-=2;
                    }
                    real b2; if(biasFactor) b2=(real)(*biasFactor)(v2[0],v2[1],v2[2]); else  b2=1.0;
                    real newDist = Eikonal<real>(d,d2,voxelsize,sofa::helper::rmin(b1,b2));
                    real oldDist = distances(v2[0],v2[1],v2[2]);
                    if(oldDist>newDist)
                    {
                        typename std::set<DistanceToPoint>::iterator it=trial.find(DistanceToPoint(oldDist,v2)); if(it!=trial.end()) trial.erase(it);
                        voronoi(v2[0],v2[1],v2[2])=vor;
                        distances(v2[0],v2[1],v2[2])=newDist;
                        trial.insert( DistanceToPoint(newDist,v2) );
                    }
                }
        }
    }
}


/**
* Update geodesic distances in the image given a bias distance function b(x).
* This is equivalent to solve for the eikonal equation || grad d(x) || = 1/b(x) with d(p)=0 at @param pos
* using dijkstra minimum path algorithm
* distances should be intialized (<0 outside the object, >=0 inside, and = 0 for seeds)
* returns @param voronoi and @param distances
*/



template<typename real,typename T>
void dijkstra (std::set<std::pair<real,sofa::defaulttype::Vec<3,int> > > &trial, CImg<real>& distances, CImg<unsigned int>& voronoi, const sofa::defaulttype::Vec<3,real>& voxelsize, const CImg<T>* biasFactor=NULL)
{
    typedef sofa::defaulttype::Vec<3,int> iCoord;
    typedef std::pair<real,iCoord > DistanceToPoint;
    const iCoord dim(distances.width(),distances.height(),distances.depth());

    //CImg<bool> alive(dim[0],dim[1],dim[2]); alive.fill(false);

    // init
    sofa::defaulttype::Vec<27,  iCoord > offset; // image coord offsets related to neighbors
    sofa::defaulttype::Vec<27,  real > lD;      // precomputed local distances (supposing that the transformation is linear)
    int count=0; for (int k=-1; k<=1; k++) for (int j=-1; j<=1; j++) for (int i=-1; i<=1; i++)
    {
        offset[count]= iCoord(i,j,k);
        lD[count]= (voxelsize.linearProduct(offset[count])).norm();
        count++;
    }
    unsigned int nbOffset=offset.size();

    // dijkstra
    while( !trial.empty() )
    {
        DistanceToPoint top = *trial.begin();
        trial.erase(trial.begin());
        iCoord v = top.second;
        //alive(v[0],v[1],v[2])=true;

        unsigned int vor = voronoi(v[0],v[1],v[2]);
        real b1; if(biasFactor) b1=(real)(*biasFactor)(v[0],v[1],v[2]); else  b1=1.0;

        for (unsigned int i=0; i<nbOffset; i++)
        {
            iCoord v2 = v + offset[i];
            if(v2[0]>=0) if(v2[1]>=0) if(v2[2]>=0) if(v2[0]<dim[0]) if(v2[1]<dim[1]) if(v2[2]<dim[2])
                //if(!alive(v2[0],v2[1],v2[2]))
            {
                real b2; if(biasFactor) b2=(real)(*biasFactor)(v2[0],v2[1],v2[2]); else  b2=1.0;
                real newDist = distances(v[0],v[1],v[2]) + lD[i]*1.0/sofa::helper::rmin(b1,b2);
                real oldDist = distances(v2[0],v2[1],v2[2]);
                if(oldDist>newDist)
                {
                    typename std::set<DistanceToPoint>::iterator it=trial.find(DistanceToPoint(oldDist,v2)); if(it!=trial.end()) trial.erase(it);
                    voronoi(v2[0],v2[1],v2[2]) = vor;
                    distances(v2[0],v2[1],v2[2]) = newDist;
                    trial.insert( DistanceToPoint(newDist,v2) );
                }
            }
        }
    }
}

///Utility functions for parallel marching method

template<typename real>
real norm(cimg_library::CImg<real>& distances, std::array<int,3>& coord)
{
    return sqrt(pow(distances(coord[0],coord[1],coord[2],0),2) +
            pow(distances(coord[0],coord[1],coord[2],1),2) +
            pow(distances(coord[0],coord[1],coord[2],2),2));
}

template<typename real,typename T>
void replace(cimg_library::CImg<unsigned int>& voronoi, cimg_library::CImg<real>& distances, std::array<int,3>& oldCoord, std::array<int,3>& newCoord, std::array<real,3>& offset, const sofa::defaulttype::Vec<3,real>& voxelSize, const CImg<T>* bias)
{
    real b=1.0;
    if(bias)
        b=std::min((*bias)(oldCoord[0], oldCoord[1], oldCoord[2]), (*bias)(newCoord[0], newCoord[1], newCoord[2]));
    distances(oldCoord[0], oldCoord[1], oldCoord[2], 0) = distances(newCoord[0], newCoord[1], newCoord[2], 0) +  offset[0]*voxelSize[0]/b;
    distances(oldCoord[0], oldCoord[1], oldCoord[2], 1) = distances(newCoord[0], newCoord[1], newCoord[2], 1) +  offset[1]*voxelSize[1]/b;
    distances(oldCoord[0], oldCoord[1], oldCoord[2], 2) = distances(newCoord[0], newCoord[1], newCoord[2], 2) +  offset[2]*voxelSize[2]/b;
    voronoi(oldCoord[0], oldCoord[1], oldCoord[2]) = voronoi(newCoord[0], newCoord[1], newCoord[2]);
}

template<typename real,typename T>
void update(cimg_library::CImg<real>& distances, cimg_library::CImg<unsigned int>& voronoi, std::array< std::array<int,3>,10 >& coord, std::array< std::array<real,3>, 10>& offset, const sofa::defaulttype::Vec<3,real>& voxelSize, const cimg_library::CImg<T>* bias)
{
    real l_curr=norm(distances,coord[0]);
    for(int l=1; l<=9; ++l)
    {
        real l_neigh=norm(distances,coord[l]);
        if(l_neigh<l_curr){ replace(voronoi,distances,coord[0],coord[l],offset[l], voxelSize, bias); l_curr=l_neigh; }
    }
}

template<typename real>
bool hasConverged(cimg_library::CImg<real>& previous, cimg_library::CImg<real>& current, SReal tolerance)
{
    bool result=true;
#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
    for(int i=0; i<previous.width(); ++i) for(int j=0; j<previous.height(); ++j) for(int k=0; k<previous.depth(); ++k)
    {
        if( !std::isnan(previous(i,j,k,0)) && !std::isnan(current(i,j,k,0)) )
        {
            SReal error = sqrt( pow(previous(i,j,k,0)-current(i,j,k,0),2) +
                                pow(previous(i,j,k,1)-current(i,j,k,1),2) +
                                pow(previous(i,j,k,2)-current(i,j,k,2),2));
            if(error>tolerance)
                result = false;
        }
    }
    return result;
}

template<typename real,typename T>
void left(CImg<unsigned int>& v, CImg<real>& d, const sofa::defaulttype::Vec<3,real>& vx, const CImg<T>* bias)
{
    for(int i=d.width()-2; i>=0; --i)
    {
#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
        for(int j=d.height()-2; j>=1; --j)
        {
            for(int k=d.depth()-2; k>=1; --k)
            {
                std::array< std::array<int,3>, 10 > c = {{ {{i,j,k}},
                                                 {{i+1, j-1, k-1}}, {{i+1, j-1, k}}, {{i+1, j-1, k+1}},
                                                 {{i+1, j, k-1}}, {{i+1, j, k}}, {{i+1, j, k+1}},
                                                 {{i+1, j+1, k-1}}, {{i+1, j+1, k}}, {{i+1, j+1, k+1}} }};
                std::array< std::array<real,3>, 10 > o = {{ {{0,0,0}},
                                                   {{1, 1, 1}}, {{1, 1, 0}}, {{1, 1, 1}},
                                                   {{1, 0, 1}}, {{1, 0, 0}}, {{1, 0, 1}},
                                                   {{1, 1, 1}}, {{1, 1, 0}}, {{1, 1, 1}} }};
                update(d,v,c,o,vx, bias);
            }
        }
    }
}

template<typename real,typename T>
void right(CImg<unsigned int>& v, CImg<real>& d, const sofa::defaulttype::Vec<3,real>& vx, const CImg<T>* bias)
{
    for(int i=1; i<d.width(); ++i)
    {
#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
        for(int j=1; j<d.height()-1; ++j)
        {
            for(int k=1; k<d.depth()-1; ++k)
            {
                std::array< std::array<int,3>, 10 > c = {{ {{i,j,k}},
                                                 {{i-1,j-1,k-1}}, {{i-1,j-1,k}}, {{i-1,j-1,k+1}},
                                                 {{i-1,j,k-1}}, {{i-1,j,k}}, {{i-1,j,k+1}},
                                                 {{i-1,j+1,k-1}}, {{i-1,j+1,k}}, {{i-1,j+1,k+1}} }};
                std::array< std::array<real,3>, 10 > o = {{ {{0,0,0}},
                                                   {{1,1,1}}, {{1,1,0}}, {{1,1,1}},
                                                   {{1,0,1}}, {{1,0,0}}, {{1,0,1}},
                                                   {{1,1,1}}, {{1,1,0}}, {{1,1,1}} }};
                update(d,v,c,o,vx, bias);
            }
        }
    }
}

template<typename real,typename T>
void down(CImg<unsigned int>& v, CImg<real>& d, const sofa::defaulttype::Vec<3,real>& vx, const CImg<T>* bias)
{
    for(int j=d.height()-2; j>=0; --j)
    {
#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
        for(int i=d.width()-2; i>=1; --i)
        {
            for(int k=d.depth()-2; k>=1; --k)
            {
                std::array< std::array<int,3>, 10> c = {{ {{i,j,k}},
                                                {{i-1, j+1, k-1}}, {{i-1, j+1, k}}, {{i-1, j+1, k+1}},
                                                {{i, j+1, k-1}}, {{i, j+1, k}}, {{i, j+1, k+1}},
                                                {{i+1, j+1, k-1}}, {{i+1, j+1, k}}, {{i+1, j+1, k+1}} }};
                std::array< std::array<real,3>, 10> o = {{ {{0,0,0}},
                                                  {{1, 1, 1}}, {{1, 1, 0}}, {{1, 1, 1}},
                                                  {{0, 1, 1}}, {{0, 1, 0}}, {{0, 1, 1}},
                                                  {{1, 1, 1}}, {{1, 1, 0}}, {{1, 1, 1}} }};
                update(d,v,c,o,vx, bias);
            }
        }
    }
}

template<typename real,typename T>
void up(CImg<unsigned int>& v, CImg<real>& d, const sofa::defaulttype::Vec<3,real>& vx, const CImg<T>* bias)
{
    for(int j=1; j<d.height(); ++j)
    {
#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
        for(int i=1; i<d.width()-1; ++i)
        {
            for(int k=1; k<d.depth()-1; ++k)
            {
                std::array< std::array<int,3>, 10> c = {{ {{i,j,k}},
                                                {{i-1, j-1, k-1}}, {{i-1, j-1, k}}, {{i-1, j-1, k+1}},
                                                {{i, j-1, k-1}}, {{i, j-1, k}}, {{i, j-1, k+1}},
                                                {{i+1, j-1, k-1}}, {{i+1, j-1, k}}, {{i+1, j-1, k+1}} }};
                std::array< std::array<real,3>, 10> o = {{ {{0,0,0}},
                                                  {{1, 1, 1}}, {{1, 1, 0}}, {{1, 1, 1}},
                                                  {{0, 1, 1}}, {{0, 1, 0}}, {{0, 1, 1}},
                                                  {{1, 1, 1}}, {{1, 1, 0}}, {{1, 1, 1}} }};
                update(d,v,c,o,vx, bias);
            }
        }
    }
}

template<typename real,typename T>
void backward(CImg<unsigned int>& v, CImg<real>& d, const sofa::defaulttype::Vec<3,real>& vx, const CImg<T>* bias)
{
    for(int k=d.depth()-2; k>=0; --k)
    {
#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
        for(int i=d.width()-2; i>=1; --i)
        {
            for(int j=d.height()-2; j>=1; --j)
            {
                std::array< std::array<int,3>, 10> c = {{ {{i,j,k}},
                                                {{i-1, j-1, k+1}}, {{i-1, j, k+1}}, {{i-1, j+1, k+1}},
                                                {{i, j-1, k+1}}, {{i, j, k+1}}, {{i, j+1, k+1}},
                                                {{i+1, j-1, k+1}}, {{i+1, j, k+1}}, {{i+1, j+1, k+1}}
                                              }};
                std::array< std::array<real,3>, 10> o = {{ {{0,0,0}},
                                                  {{1, 1, 1}}, {{1, 0, 1}}, {{1, 1, 1}},
                                                  {{0, 1, 1}}, {{0, 0, 1}}, {{0, 1, 1}},
                                                  {{1, 1, 1}}, {{1, 0, 1}}, {{1, 1, 1}}
                                                }};
                update(d,v,c,o,vx,bias);
            }
        }
    }
}

template<typename real,typename T>
void forward(CImg<unsigned int>& v, CImg<real>& d, const sofa::defaulttype::Vec<3,real>& vx, const CImg<T>* bias)
{
    for(int k=1; k<d.depth(); ++k)
    {
#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
        for(int i=1; i<d.width()-1; ++i)
        {
            for(int j=1; j<d.height()-1; ++j)
            {
                std::array< std::array<int,3>, 10> c = {{ {{i,j,k}},
                                                {{i-1, j-1, k-1}}, {{i-1, j, k-1}}, {{i-1, j+1, k-1}},
                                                {{i, j-1, k-1}}, {{i, j, k-1}}, {{i, j+1, k-1}},
                                                {{i+1, j-1, k-1}}, {{i+1, j, k-1}}, {{i+1, j+1, k-1}} }};
                std::array< std::array<real,3>, 10> o = {{ {{0,0,0}},
                                                  {{1, 1, 1}}, {{1, 0, 1}}, {{1, 1, 1}},
                                                  {{0, 1, 1}}, {{0, 0, 1}}, {{0, 1, 1}},
                                                  {{1, 1, 1}}, {{1, 0, 1}}, {{1, 1, 1}} }};
                update(d,v,c,o,vx,bias);
            }
        }
    }
}

template<typename real,typename T>
void rasterScan(cimg_library::CImg<unsigned int>& voronoi, cimg_library::CImg<real>& distances, const sofa::defaulttype::Vec<3,real>& voxelSize, const cimg_library::CImg<T>* biasFactor=NULL)
{
    right(voronoi, distances, voxelSize, biasFactor);
    left(voronoi, distances, voxelSize, biasFactor);
    down(voronoi, distances, voxelSize,biasFactor);
    up(voronoi, distances, voxelSize,biasFactor);
    forward(voronoi, distances, voxelSize,biasFactor);
    backward(voronoi, distances, voxelSize,biasFactor);
}

template<typename real,typename T>
void parallelMarching(cimg_library::CImg<real>& distances, cimg_library::CImg<unsigned int>& voronoi, const sofa::defaulttype::Vec<3,real>& voxelSize, const unsigned int maxIter=1e10, const SReal tolerance=10, const cimg_library::CImg<T>* biasFactor=NULL)
{
    //Build a new distance image from distances.
    cimg_library::CImg<real> v_distances(distances.width(), distances.height(), distances.depth(), 3, std::numeric_limits<real>::max());
#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
    for(int i=0; i<distances.width(); ++i) for(int j=0; j<distances.height(); ++j) for(int k=0; k<distances.depth(); ++k)
    {
        if( distances(i,j,k,0) < 0 )
            v_distances(i,j,k,0) = v_distances(i,j,k,1) = v_distances(i,j,k,2) = std::numeric_limits<real>::signaling_NaN();
        else
            v_distances(i,j,k,0) = v_distances(i,j,k,1) = v_distances(i,j,k,2) = distances(i,j,k,0);
    }

    //Perform raster scan until convergence
    bool converged = false; int iter_count = 0; cimg_library::CImg<real> prev_distances;
    while( (converged==false) || (iter_count<maxIter) )
    {
        prev_distances = v_distances; iter_count++;
        rasterScan(voronoi, v_distances, voxelSize, biasFactor);
        converged = hasConverged(prev_distances, v_distances, tolerance);
    }

    //Update distances with v_distances
#ifdef USING_OMP_PRAGMAS
#pragma omp parallel for
#endif
    for(int i=0; i<distances.width(); ++i) for(int j=0; j<distances.height(); ++j) for(int k=0; k<distances.depth(); ++k)
    {
        if( v_distances(i,j,k,0)==std::numeric_limits<real>::signaling_NaN() )
            distances(i,j,k,0) = -1.0;
        else
            distances(i,j,k,0) = std::sqrt( std::pow(v_distances(i,j,k,0),2) + std::pow(v_distances(i,j,k,1),2) + std::pow(v_distances(i,j,k,2),2) );
    }
}

/**
* Initialize null distances and voronoi value (=point index) from a position in image coordinates
* and returns list of seed (=trial) points to be used in dijkstra or fast marching algorithms
*/

template<typename real>
void AddSeedPoint (std::set<std::pair<real,sofa::defaulttype::Vec<3,int> > >& trial, CImg<real>& distances, CImg<unsigned int>& voronoi, const sofa::defaulttype::Vec<3,real>& pos,  const unsigned int index)
{
    typedef sofa::defaulttype::Vec<3,int> iCoord;
    typedef std::pair<real,iCoord > DistanceToPoint;

    iCoord P;  for (unsigned int j=0; j<3; j++)  P[j]=sofa::helper::round(pos[j]);
    if(distances.containsXYZC(P[0],P[1],P[2]))
        if(distances(P[0],P[1],P[2])>=0)
        {
            distances(P[0],P[1],P[2])=0;
            voronoi(P[0],P[1],P[2])=index;
            trial.insert( DistanceToPoint(0.,iCoord(P[0],P[1],P[2])) );
        }
}




#endif // IMAGEALGORITHMS_H
