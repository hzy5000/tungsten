// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "quadv.h"
#include "quad_intersector_moeller.h"

namespace embree
{
  namespace isa
  {
    /*! Intersects 4 quads with 1 ray */
    template<int M, bool filter>
      struct QuadMvIntersector1MoellerTrumbore
    {
      typedef QuadMv<M> Primitive;
      typedef QuadMIntersector1MoellerTrumbore<M,filter> Precalculations;
        
      /*! Intersect a ray with the M quads and updates the hit. */
      static __forceinline void intersect(const Precalculations& pre, Ray& ray, const RTCIntersectContext* context, const Primitive& quad, Scene* scene, const unsigned* geomID_to_instID)
      {
        STAT3(normal.trav_prims,1,1,1);
        pre.intersect(ray,context,quad.v0,quad.v1,quad.v2,quad.v3,quad.geomIDs,quad.primIDs,scene,geomID_to_instID);
      }
        
      /*! Test if the ray is occluded by one of M quads. */
      static __forceinline bool occluded(const Precalculations& pre, Ray& ray, const RTCIntersectContext* context, const Primitive& quad, Scene* scene, const unsigned* geomID_to_instID)
      {
        STAT3(shadow.trav_prims,1,1,1);
        return pre.occluded(ray,context, quad.v0,quad.v1,quad.v2,quad.v3,quad.geomIDs,quad.primIDs,scene,geomID_to_instID);
      }

      /*! Intersect an array of rays with an array of M primitives. */
      static __forceinline size_t intersect(Precalculations* pre, size_t valid, Ray** rays, const RTCIntersectContext* context,  size_t ty, const Primitive* prim, size_t num, Scene* scene, const unsigned* geomID_to_instID)
      {
        size_t valid_isec = 0;
        do {
          const size_t i = __bscf(valid);
          const float old_far = rays[i]->tfar;
          for (size_t n=0; n<num; n++)
            intersect(pre[i],*rays[i],context,prim[n],scene,geomID_to_instID);
          valid_isec |= (rays[i]->tfar < old_far) ? ((size_t)1 << i) : 0;            
        } while(unlikely(valid));
        return valid_isec;
      }

    };

    /*! Intersects M triangles with K rays. */
    template<int M, int K, bool filter>
      struct QuadMvIntersectorKMoellerTrumbore
      {
        typedef QuadMv<M> Primitive;
        typedef QuadMIntersectorKMoellerTrumbore<M,K,filter> Precalculations;
        
        /*! Intersects K rays with M triangles. */
        static __forceinline void intersect(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, const RTCIntersectContext* context, const QuadMv<M>& quad, Scene* scene)
        {
          for (size_t i=0; i<QuadMv<M>::max_size(); i++)
          {
            if (!quad.valid(i)) break;
            STAT3(normal.trav_prims,1,popcnt(valid_i),K);
            const Vec3<vfloat<K>> p0 = broadcast<vfloat<K>>(quad.v0,i);
            const Vec3<vfloat<K>> p1 = broadcast<vfloat<K>>(quad.v1,i);
            const Vec3<vfloat<K>> p2 = broadcast<vfloat<K>>(quad.v2,i);
            const Vec3<vfloat<K>> p3 = broadcast<vfloat<K>>(quad.v3,i);
            pre.intersectK(valid_i,ray,p0,p1,p2,p3,IntersectKEpilogM<M,K,filter>(ray,context,quad.geomIDs,quad.primIDs,i,scene));
          }
        }
        
        /*! Test for K rays if they are occluded by any of the M triangles. */
        static __forceinline vbool<K> occluded(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, const RTCIntersectContext* context, const QuadMv<M>& quad, Scene* scene)
        {
          vbool<K> valid0 = valid_i;
          
          for (size_t i=0; i<QuadMv<M>::max_size(); i++)
          {
            if (!quad.valid(i)) break;
            STAT3(shadow.trav_prims,1,popcnt(valid0),K);
            const Vec3<vfloat<K>> p0 = broadcast<vfloat<K>>(quad.v0,i);
            const Vec3<vfloat<K>> p1 = broadcast<vfloat<K>>(quad.v1,i);
            const Vec3<vfloat<K>> p2 = broadcast<vfloat<K>>(quad.v2,i);
            const Vec3<vfloat<K>> p3 = broadcast<vfloat<K>>(quad.v3,i);
            if (pre.intersectK(valid0,ray,p0,p1,p2,p3,OccludedKEpilogM<M,K,filter>(valid0,ray,context,quad.geomIDs,quad.primIDs,i,scene))) 
              break;
          }
          return !valid0;
        }
        
        /*! Intersect a ray with M triangles and updates the hit. */
        static __forceinline void intersect(Precalculations& pre, RayK<K>& ray, size_t k, const RTCIntersectContext* context, const QuadMv<M>& quad, Scene* scene)
        {
          STAT3(normal.trav_prims,1,1,1);
          pre.intersect1(ray,k,context,quad.v0,quad.v1,quad.v2,quad.v3,quad.geomIDs,quad.primIDs,scene); 
        }
        
        /*! Test if the ray is occluded by one of the M triangles. */
        static __forceinline bool occluded(Precalculations& pre, RayK<K>& ray, size_t k, const RTCIntersectContext* context, const QuadMv<M>& quad, Scene* scene)
        {
          STAT3(shadow.trav_prims,1,1,1);
          return pre.occluded1(ray,k,context,quad.v0,quad.v1,quad.v2,quad.v3,quad.geomIDs,quad.primIDs,scene); 
        }
      };
  }
}

