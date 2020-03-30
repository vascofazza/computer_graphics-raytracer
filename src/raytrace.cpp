#include "scene.h"
#include "intersect.h"
#include "common.h"

#define MAX_REC 8

ray3f get_refract_ray(const ray3f, const vec3f, vec3f, float &, float);

// compute the color corresponing to a ray by raytracing
vec3f raytrace_ray(Scene* scene, ray3f ray, int R) {
    
    if(R < 0)
        return zero3f;
    // get scene intersection
    intersection3f intersection = intersect_surfaces(scene, ray);
    // if not hit, return background
    if(!intersection.hit)
        return scene->background;
    
    // accumulate color starting with ambient
    vec3f color = (scene->ambient)*intersection.mat->kd;
    color+= intersection.mat->ke;
    vec3f rayInverseDir = normalize(ray.e-intersection.pos); //non e' la stessa cosa
    Material mat = *intersection.mat;
    // foreach light
    for(Light* light : scene->lights)
    {
        // compute light response
        vec3f lightIntensity = light->intensity / distSqr(light->frame.o, intersection.pos);
        if(lightIntensity == zero3f) continue;
        
        // compute light direction
        vec3f lToPos = light->frame.o-intersection.pos;
        vec3f ldir = normalize(lToPos);
        //vec3f mirror = normalize(-rayInverseDir + 2*(dot(intersection.norm,rayInverseDir))*intersection.norm);
        
        // compute the material response (brdf*cos)
        
        //auto diffuse = mat.kd * max(0.0f, (dot(intersection.norm, ldir)));
        //brdf += mat.ks * max(0.0f, pow(dot(intersection.norm, bisector),mat.n))*abs(dot(intersection.norm, ldir));
        //auto specular =  mat.ks * max(0.0f, pow(dot(intersection.norm, bisector),mat.n))*abs(dot(intersection.norm, ldir));
        //vec3f col = lightRadiance * (mat.kd + mat.ks * max(0.0f, pow(dot(intersection.norm, bisector),mat.n))) *abs(dot(intersection.norm, ldir));// * (diffuse +specular);
        
        // check for shadows and accumulate if needed
        ray3f shadow = ray3f(intersection.pos, ldir, ray3f_epsilon, length(lToPos));
        //int visibility = 0;
        if(!intersect_surfaces(scene, shadow).hit)
        {
            vec3f bisector = normalize(ldir+rayInverseDir);
            //accumulate
            color+= (lightIntensity * /*visibility **/ (mat.kd + mat.ks * max(0.0f, pow(dot(intersection.norm, bisector),mat.n))) * max(0.0f, dot(intersection.norm, ldir)));
        }
    }
    // if the material has refraction
    if(!(mat.kt == zero3f))
    {
        //rifrazione dopo il primo raggio è sempre 1 sennò muore la potenza
        //vec3f vRefr = refract(ray.d, intersection.norm, 1/mat.theta);
        float refrK = 1;
        auto rayRefr = get_refract_ray(ray, intersection.pos, intersection.norm, refrK, mat.theta);//ray3f(intersection.pos, vRefr);
        auto refr = mat.kt * raytrace_ray(scene, rayRefr, R-1);
        color += refrK * refr;
    }
    // if the material has reflections
    // create the reflection ray
    if(!(mat.kr == zero3f))
    {
        vec3f mirror = normalize(ray.d - 2*(dot(intersection.norm,ray.d))*intersection.norm);
        auto refl = mat.kr * raytrace_ray(scene, ray3f(intersection.pos, mirror), R-1);
        // accumulate the reflected light (recursive call) scaled by the material reflection
        color += refl;
    }
    // return the accumulated color∫
    return color;
}

vec3f raytrace_ray(Scene* scene, ray3f ray)
{
    return raytrace_ray(scene, ray, MAX_REC);
}

ray3f get_refract_ray(const ray3f r, const vec3f intersection, vec3f normal, float &trans, float angle)
{
    float n1, n2, n;
    float cosI = dot(r.d,normal);
    if(cosI > 0.0)
    {
        n1 = angle;
        n2 = 1.0;
        normal = -normal;
    }
    else
    {
        n1 = 1.0;
        n2 = angle;
        cosI = -cosI;
    }
    n = n1/n2;
    float sinT2 = n*n * (1.0 - cosI * cosI);
    float cosT = sqrt(1.0 - sinT2);
    //fresnel equations
    float rn = (n1 * cosI - n2 * cosT)/(n1 * cosI + n2 * cosT);
    float rt = (n2 * cosI - n1 * cosT)/(n2 * cosI + n2 * cosT);
    rn *= rn;
    rt *= rt;
    //refl = (rn + rt)*0.5;
    //trans = 1.0 - refl;
    if(n == 1.0)
        return r;
    if(cosT*cosT < 0.0)//tot inner refl
    {
        //refl = 1;
        trans = 0;
        vec3f mirror = normalize(r.d - 2*(dot(normal,r.d))*normal);
        return ray3f(intersection, mirror);
    }
    vec3f dir = n * r.d + (n * cosI - cosT)*normal;
    return ray3f(intersection + dir * 1, dir);
    //refraction-in-raytracing
}

vec3f get_direction(Scene* scene, float u, float v, float ratio)
{
    auto q = vec3f((u -0.5f)*ratio, (v -0.5f)*1 , - scene->camera->dist);
    return normalize(q);
}

// raytrace an image
image3f raytrace(Scene* scene) {
    // allocate an image of the proper size
    auto image = image3f(scene->image_width, scene->image_height);
    float ratio = scene->image_width/(float)scene->image_height;
    //determine view direction
    if(scene->image_samples > 1)
    {
        for(unsigned x : range(scene->image_width))
            for(unsigned y : range(scene->image_height))
            {
                vec3f color = zero3f;
                for(unsigned xx : range(scene->image_samples))
                {
                    for(unsigned yy : range(scene->image_samples))
                    {
                        float u = (x+(xx+.5f)/scene->image_samples)/scene->image_width;
                        float v = (y+(yy+.5f)/scene->image_samples)/scene->image_height;
                        vec3f direction = get_direction(scene, u, v, ratio);
                        ray3f ray = transform_ray(scene->camera->frame, ray3f(zero3f, direction));
                        color += raytrace_ray(scene, ray);
                    }
                }
                image.at(x,y) = color/pow(scene->image_samples,2);
            }
    }
    else
    {
        for(unsigned x : range(scene->image_width))
            for(unsigned y : range(scene->image_height))
            {
                float u = (x+.5f) / scene->image_width; //center of the pixel
                float v = (y+.5f) / scene->image_height; //center of the pixel
                vec3f direction = get_direction(scene, u,v, ratio);
                ray3f ray = transform_ray(scene->camera->frame,ray3f(zero3f, direction)); //zero perchè trasformo dall'origine
                vec3f trace = raytrace_ray(scene, ray);
                image.at(x, y) = trace;
            }
    }
    
    
    // if no anti-aliasing
        // foreach pixel
                // compute ray-camera parameters (u,v) for the pixel
                // compute camera ray
                // set pixel to the color raytraced with the ray
    // else
        // foreach pixel
                // init accumulated color
                // foreach sample
                        // compute ray-camera parameters (u,v) for the pixel and the sample
                        // compute camera ray
                        // set pixel to the color raytraced with the ray
                // scale by the number of samples
    
    // done
    return image;
}

bool check(image3f* im1, image3f* im2)
{
    int c = 0;
    for(int i : range(im1->height()))
        for(int j : range(im1->width()))
        {
            auto m1 = im1->at(i, j);
            auto m2 = im2->at(i, j);
            if(!(m1 == m2))
            {
                c++;//return false;
                message("%d, %d\t%f, %f\t%f, %f\t%f, %f\n", j, i, m1.x, m2.x, m1.y, m2.y, m1.z, m2.z);
            }
        }
    message("\t%d\t", c);
    return true;
}

inline bool exists_test (const std::string& name) {
    if (FILE *file = fopen(name.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}
// runs the raytrace over all tests and saves the corresponding images
int main(int argc, char** argv) {
    auto args = parse_cmdline(argc, argv,
        { "01_raytrace", "raytrace a scene",
            {  {"resolution", "r", "image resolution", "int", true, jsonvalue() }  },
            {  {"scene_filename", "", "scene filename", "string", false, jsonvalue("scene.json")},
               {"image_filename", "", "image filename", "string", true, jsonvalue("")}  }
        });
    auto scene_filename = args.object_element("scene_filename").as_string();
    auto image_filename = (args.object_element("image_filename").as_string() != "") ?
        args.object_element("image_filename").as_string() :
        scene_filename.substr(0,scene_filename.size()-5)+".png";
    auto check_image_filename = (args.object_element("image_filename").as_string() != "") ?
    args.object_element("image_filename").as_string() :
    scene_filename.substr(0,scene_filename.size()-5)+".check.png";
    auto scene = load_json_scene(scene_filename);
    if(not args.object_element("resolution").is_null()) {
        scene->image_height = args.object_element("resolution").as_int();
        scene->image_width = scene->camera->width * scene->image_height / scene->camera->height;
    }
    message("rendering %s ... ", scene_filename.c_str());
    auto image = raytrace(scene);
    write_png(image_filename, image, true);
    if(exists_test(check_image_filename))
    {
        image = read_png(image_filename, true);
        auto image2 = read_png(check_image_filename, true);
        check(&image, &image2);
    }
    delete scene;
    message("done\n");
}
