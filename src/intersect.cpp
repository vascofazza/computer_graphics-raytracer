#include "scene.h"
#include "intersect.h"

float intersect_sphere(Surface*, vec3f*, vec3f*, ray3f*);
float intersect_plane(Surface*, ray3f*);
float intersect_cylinder(Surface*, vec3f*, vec3f*, ray3f*);
    // intersects the scene's surfaces and return the first intrerseciton (used for raytracing homework)
    intersection3f intersect_surfaces(Scene* scene, ray3f ray) {
    // create a default intersection record to be returned
    auto intersection = intersection3f();
    intersection.ray_t = MAXFLOAT;
    vec3f pos,n;
    float t;
    
    // foreach surface
    for(Surface* s: scene->surfaces)
    {
        bool hit = false;
        // if it is a quad
        switch (s->shape)
        {
            //if it is a cylinder
            case 2:
            {
                t = intersect_cylinder(s, &pos, &n, &ray);
                if(t > ray.tmin && t < intersection.ray_t && t < ray.tmax)
                {
                    hit = true;
                }
                pos = transform_point(s->frame, pos);
                n = transform_normal(s->frame, n);
                break;
            }
            // if it is a plane
            case 1:
            {
                // compute ray intersection (and ray parameter), continue if not hit
//                vec3f n1 = transform_normal(s->frame, z3f);
                t = intersect_plane(s, &ray);
                // check if computed param is within ray.tmin and ray.tmax
                if(t > ray.tmin && t < intersection.ray_t && t < ray.tmax)
                {
                        // check if this is the closest intersection, continue if
                        // if hit, set intersection record values
                        pos = ray.eval(t);
                        auto dist = (pos-s->frame.o);
                        if(fabs(dist.x) > s->radius || fabs(dist.y) > s->radius || fabs(dist.z) > s->radius)
                            break;
                        hit = true;
                        n = s->frame.z;
                }
                break;
            }
            // if it is a sphere
            case 0:
            {
                // compute ray intersection (and ray parameter), continue if not hit
                t = intersect_sphere(s, &pos, &n, &ray);
                // check if computed param is within ray.tmin and ray.tmax
                if(t > ray.tmin && t < intersection.ray_t && t < ray.tmax)
                {
                    // check if this is the closest intersection, continue if not
                    // if hit, set intersection record values
                    hit = true;
                    //normalize(intersection.pos-(s->frame.o));
                }
                break;
            }
        }
        // record closest intersection
        if(hit) //float comparison fix
        {
            intersection.hit = true;
            intersection.ray_t = t;
            intersection.pos = pos;
            intersection.mat = s->mat;
            intersection.norm = n;
        }
    }
    return intersection;
}

float intersect_cylinder(Surface* s, vec3f* pos, vec3f* n, ray3f* ray)
{
    auto old_ray = *ray;
    old_ray = transform_ray_inverse(s->frame, old_ray);
    ray = &old_ray;
    vec3f d = ray->d;
    vec3f e = ray->e;
    vec3f C = zero3f;//s->frame.o;
    vec3f p = e-C;
    float a = pow(d.x,2) + pow(d.z,2);
    float b = 2*(d.x*p.x + d.z*p.z);
    float c = pow(p.x,2) + pow(p.z,2) - pow(s->radius,2);
    
    float disc = b*b - 4*a*c;
    
    if(disc >= 0)
    {
        float pH = C.y + s->height/2.f;
        float mH = C.y - s->height/2.f;
        float t0 = (-b - sqrt(disc)) / (2 * a);
        float t1 = (-b + sqrt(disc)) / (2 * a);

        if (t0>t1) {float tmp = t0;t0=t1;t1=tmp;}
        
        float y0 = (*ray).eval(t0).y;
        float y1 = (*ray).eval(t1).y;
        
        //intersezione col tappo superiore se la prima intersezione è superiore al tappo e la seconda inferiore al tappo
        if(y0 > pH)
        {
            //gestire il riflesso della luce che sbatte contro le pareti interne
            if(y1 < pH || y1 < mH) // da tappo a tappo
            {
                *n = y3f;
                vec3f pp = s->frame.o;
                pp.y = pH;
                const float den = dot(d, *n);
                const float num = dot(*n,(pp-e));
                const float t = num/den;
                *pos = (*ray).eval(t);
                return t;
            }
            return -1;
        }
        
        if(y0 < mH)
        {
            if(y1>mH || y1 > pH)
            {
                *n = -y3f;
                vec3f pp = s->frame.o;
                pp.y = mH;
                const float den = dot(d, *n);
                const float num = dot(*n,(pp-e));
                const float t = num/den;
                *pos = (*ray).eval(t);
                return t;
            }
            return -1;
        }
        
        *pos = (*ray).eval(t0);
        *n = (*pos) - C;
        n->y = 0;
        *n = normalize(*n);
        return t0;
        
        if (y0<mH)
        {
            if (y1<mH)
                return -1;
            else
            {
                // hit the cap
                float th = t0 + (t1-t0) * (y0-pH) / (y0-y1);
                if (th<=0) return -1;
                *pos = (*ray).eval(th);
                *n = -y3f;
                return th;
            }
        }
        else if (y0>=mH && y0<=pH)
        {
            // hit the cylinder bit
            if (t0<0) return -1;
            
            *pos = (*ray).eval(t0);
            *n = vec3f(*pos);
            n->y = 0;
            *n = normalize(*n);
            return t0;
        }
        else if (y0>pH)
        {
            if (y1>pH)
                return -1;
            else
            {
                // hit the cap
                float th = t0 + (t1-t0) * (y0+pH) / (y0-y1);
                if (th<=0) return -1;
                
                *pos = (*ray).eval(th);
                *n = y3f;
                return th;
            }
        }
        
        return -1;
    }
    return -1;
}

float intersect_plane(Surface* s, ray3f* ray)
{
    vec3f n = s->frame.z;
    const float den = dot(ray->d, n);
    const float num = dot(n,(s->frame.o-ray->e));
    const float t = num/den;
    return t;
}

//la soluzione è t (ray(t))
float intersect_sphere(Surface* s, vec3f* pos, vec3f* n, ray3f* ray)
{
    vec3f d = ray->d;
    vec3f e = ray->e;
    vec3f L = (e-(s->frame.o));
    float a = lengthSqr(d);
    float b = 2*dot(d, L);
    float c = lengthSqr(L) - (s->radius*s->radius);
    
    float determinant = b*b - 4*a*c;
    if(determinant < 0)
        return -1;
    else if(determinant == 0)
        return -b/(2*a);
    else
    {
        float t = 0;
        auto t0 = (-b+sqrt(determinant))/(2*a);
        auto t1 = (-b-sqrt(determinant))/(2*a);
        if(t0 < ray->tmin || t1 < ray->tmin) //inside sphere
            t = max(t0,t1);
        else
            t = min(t0,t1);
        *pos = (*ray).eval(t);
        *n = normalize((*pos - s->frame.o)/s->radius);
        return t;
    }
}
