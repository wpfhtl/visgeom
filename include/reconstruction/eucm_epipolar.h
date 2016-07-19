/*
This file is part of visgeom.

visgeom is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

visgeom is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with visgeom.  If not, see <http://www.gnu.org/licenses/>.
*/ 


/*
Semi-global block matching algorithm for non-rectified images
*/

#pragma once
//STL
#include "timer.h"
#include "std.h"
#include "eigen.h"
#include "geometry/geometry.h"
#include "camera/eucm.h"

const double HALF_PI = M_PI / 2;

class EnhancedEpipolar
{
public:
    EnhancedEpipolar(Transformation<double> T12,
            const double * params1, const double * params2, const int numberSteps) :
            // initialize the members
            Transform12(T12), 
            cam1(params1), //TODO implement first image directions
            cam2(params2),
            step(4. / numberSteps),
            nSteps(numberSteps)
    {
        assert(nSteps % 2 == 0);
        Timer timer;
        //TODO put this code into setTransformation(Transformation<double>);
        zBase = -T12.trans().normalized();
        if (zBase[2]*zBase[2] > zBase[0]*zBase[0] + zBase[1]*zBase[1])
        {
            xBase << 1, 0, 0;
        }
        else
        {
            xBase << 0, 0, 1;
        }
        Matrix3d orthProjector = Matrix3d::Identity() - zBase*zBase.transpose();
        xBase = orthProjector * xBase;
        xBase.normalize();
        yBase = zBase.cross(xBase);
        
        Matrix3d R21 = Transform12.rotMatInv();
        Vector3d t21n = R21 * zBase;
        Vector3d xBase2 = R21 * xBase;
        
        const double & alpha = cam2.params[0];
        const double & beta = cam2.params[1];
        const double & fu = cam2.params[2];
        const double & fv = cam2.params[3];
        const double & u0 = cam2.params[4];
        const double & v0 = cam2.params[5];
        
        // intermediate variables
        double gamma = 1 - alpha;
        double ag = (alpha - gamma);
        double a2b = alpha*alpha*beta;
        double fufv = fu * fv;
        double fufu = fu * fu;
        double fvfv = fv * fv;
        
        cam2.projectPoint(t21n, epipole);
        
        
        for (int idx = 0; idx < numberSteps; idx++)
        {
            Vector3d X;
            if (idx < numberSteps/2)
            {
                //tangent part
                double s = step * idx - 1;
                X = xBase + s*yBase;
            }
            else
            {
                //cotangent part
                double c = step * (-idx + numberSteps/2) + 1;
                X = c * xBase + yBase;
            }
            X = R21 * X;
            epipolarVec.emplace_back();
            Polynomial2 & surf = epipolarVec.back();
            Vector3d plane = X.cross(t21n);
            const double & A = plane[0];
            const double & B = plane[1];
            const double & C = plane[2];
            double AA = A * A;
            double BB = B * B;
            double CC = C * C;
            double CCfufv = CC * fufv;
            if (CCfufv/(AA + BB) < 0.5) // the curve passes through the projection center
            {
                surf.kuu = surf.kuv = surf.kvv = 0;
                surf.ku = A/fu;
                surf.kv = B/fv;
                surf.k1 = -u0*A/fu - v0*B/fv;
            }
            else
            {
                // compute first 4 coefficients directly
                surf.kuu = (AA*ag + CC*a2b)/(CC*fufu);  // kuu
                surf.kuv = 2*A*B*ag/(CCfufv);  // kuv
                surf.kvv = (BB*ag + CC*a2b)/(CC*fvfv);  // kvv
                surf.ku = 2*(-(AA*fv*u0 + A*B*fu*v0)*ag - 
                                A*C*fufv*gamma - CC*a2b*fv*u0)/(CCfufv*fu);  // kv
                surf.kv = 2*(-(BB*fu*v0 + A*B*fv*u0)*ag - 
                                B*C*fufv*gamma - CC*a2b*fu*v0)/(CCfufv*fv);  // kv
                                
                // the last one is computed using the fact that
                // the epipolar curves pass through the epipole
                surf.k1 = -(surf.kuu*epipole[0]*epipole[0] 
                                + surf.kuv*epipole[0]*epipole[1] 
                                + surf.kvv*epipole[1]*epipole[1] 
                                + surf.ku*epipole[0] + surf.kv*epipole[1]);
            }
        }
        epipolarVec.emplace_back(epipolarVec.front());
        cout << "    epipolar init time : " << timer.elapsed() << endl;
    }
  
    int index(Vector3d X) const
    {
        double c = X.dot(xBase);
        double ac = abs(c);
        double s = X.dot(yBase);
        double as = abs(s);
        if (ac + as < 1e-4) //TODO check the constant 
        {
            return 0;
        }
        else if (ac > as)
        {
            return round((s/c + 1) / step);
        }
        else
        {
            return round((1 - c/s) / step) + nSteps/2;
        }
    }
    
    const Polynomial2 & getCurve(int idx) const { return epipolarVec[idx]; }
    const Polynomial2 & getCurve(Vector3d X) const 
    { 
        return epipolarVec[index(X)]; 
    }
    
private:
    
    
    Transformation<double> Transform12;  // pose of the first to the second camera
    EnhancedCamera cam1, cam2;
   
    // angular distance between two plains that represent the epipolar lines
    
    int nSteps; //must be even
    double step;
    
    Vector2d epipole;
    
    // the basis in which the input vector is decomposed
    Vector3d xBase, yBase, zBase;
    
    // the epipolar curves represented by polynomial functions
    // epipolarVec[0] corresponds to base rotated about t by -pi/2
    std::vector<Polynomial2> epipolarVec;  
};

