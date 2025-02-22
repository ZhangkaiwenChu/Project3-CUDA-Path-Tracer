#pragma once

#include "intersections.h"

// CHECKITOUT
/**
 * Computes a cosine-weighted random direction in a hemisphere.
 * Used for diffuse lighting.
 */
__host__ __device__
glm::vec3 calculateRandomDirectionInHemisphere(
        glm::vec3 normal, thrust::default_random_engine &rng) {
    thrust::uniform_real_distribution<float> u01(0, 1);

    float up = sqrt(u01(rng)); // cos(theta)
    float over = sqrt(1 - up * up); // sin(theta)
    float around = u01(rng) * TWO_PI;

    // Find a direction that is not the normal based off of whether or not the
    // normal's components are all equal to sqrt(1/3) or whether or not at
    // least one component is less than sqrt(1/3). Learned this trick from
    // Peter Kutz.

    glm::vec3 directionNotNormal;
    if (abs(normal.x) < SQRT_OF_ONE_THIRD) {
        directionNotNormal = glm::vec3(1, 0, 0);
    } else if (abs(normal.y) < SQRT_OF_ONE_THIRD) {
        directionNotNormal = glm::vec3(0, 1, 0);
    } else {
        directionNotNormal = glm::vec3(0, 0, 1);
    }

    // Use not-normal direction to generate two perpendicular directions
    glm::vec3 perpendicularDirection1 =
        glm::normalize(glm::cross(normal, directionNotNormal));
    glm::vec3 perpendicularDirection2 =
        glm::normalize(glm::cross(normal, perpendicularDirection1));

    return up * normal
        + cos(around) * over * perpendicularDirection1
        + sin(around) * over * perpendicularDirection2;
}

__host__ __device__
glm::vec3 imperfectReflection(glm::vec3 normal, thrust::default_random_engine& rng, float n) {
    thrust::uniform_real_distribution<float> u01(0, 1);
    glm::vec3 directionNotNormal;
    
    if (abs(normal.x) < SQRT_OF_ONE_THIRD) {
        directionNotNormal = glm::vec3(1, 0, 0);
    }
    else if (abs(normal.y) < SQRT_OF_ONE_THIRD) {
        directionNotNormal = glm::vec3(0, 1, 0);
    }
    else {
        directionNotNormal = glm::vec3(0, 0, 1);
    }

    // Use not-normal direction to generate two perpendicular directions
    glm::vec3 perpendicularDirection1 =
        glm::normalize(glm::cross(normal, directionNotNormal));
    glm::vec3 perpendicularDirection2 =
        glm::normalize(glm::cross(normal, perpendicularDirection1));
    float theta = acos(std::pow(u01(rng), 1.f / (1.f + n)));
    float phi = 2 * PI * u01(rng);
    float x = cos(phi) * sin(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(theta);
    return z * normal + x * perpendicularDirection1 + y * perpendicularDirection2;
}



/**
 * Scatter a ray with some probabilities according to the material properties.
 * For example, a diffuse surface scatters in a cosine-weighted hemisphere.
 * A perfect specular surface scatters in the reflected ray direction.
 * In order to apply multiple effects to one surface, probabilistically choose
 * between them.
 *
 * The visual effect you want is to straight-up add the diffuse and specular
 * components. You can do this in a few ways. This logic also applies to
 * combining other types of materias (such as refractive).
 *
 * - Always take an even (50/50) split between a each effect (a diffuse bounce
 *   and a specular bounce), but divide the resulting color of either branch
 *   by its probability (0.5), to counteract the chance (0.5) of the branch
 *   being taken.
 *   - This way is inefficient, but serves as a good starting point - it
 *     converges slowly, especially for pure-diffuse or pure-specular.
 * - Pick the split based on the intensity of each material color, and divide
 *   branch result by that branch's probability (whatever probability you use).
 *
 * This method applies its changes to the Ray parameter `ray` in place.
 * It also modifies the color `color` of the ray in place.
 *
 * You may need to change the parameter list for your purposes!
 */
__host__ __device__
void scatterRay(
        PathSegment & pathSegment,
        glm::vec3 intersect,
        glm::vec3 normal,
        const Material &m,
        thrust::default_random_engine &rng) {
    // TODO: implement this.
    // A basic implementation of pure-diffuse shading will just call the
    // calculateRandomDirectionInHemisphere defined above.
    if (pathSegment.remainingBounces > 0) {
        float n = m.indexOfRefraction;
        if (glm::dot(normal, pathSegment.ray.direction) > 0) {
            normal = -normal;
            n = 1.f / n;
        }
        if (m.hasRefractive) {
            thrust::uniform_real_distribution<float> u01(0, 1);
            float R0 = glm::pow((1.f - n) / (1.f + n), 2.f);
            float R = R0 + (1 - R0) * glm::pow(1.f + glm::dot(normal, pathSegment.ray.direction), 5.f);
            if (u01(rng) > R) {
                glm::vec3 out = glm::refract(pathSegment.ray.direction, normal, 1.f / n);
                if (glm::length(out) > 0.f){
                    pathSegment.color *= m.specular.color;
                    pathSegment.ray.direction = glm::normalize(out);
                    pathSegment.remainingBounces--;
                    pathSegment.ray.origin = intersect + 0.0001f * pathSegment.ray.direction;
                    if (pathSegment.remainingBounces == 0) {
                        pathSegment.color *= 0.f;
                    }
                    return;
                }
            }
        }

        if (m.hasReflective || m.hasRefractive) {
            pathSegment.color *= m.specular.color;
           pathSegment.ray.direction = glm::normalize(glm::reflect(pathSegment.ray.direction, normal));
            //pathSegment.ray.direction = glm::normalize(imperfectReflection(glm::normalize(glm::reflect(pathSegment.ray.direction, normal)), rng, m.shininess));
            pathSegment.remainingBounces--;
            pathSegment.ray.origin = intersect + 0.0001f * pathSegment.ray.direction;
        }

        else {
            pathSegment.color *= m.color;
            pathSegment.ray.direction = glm::normalize(calculateRandomDirectionInHemisphere(normal, rng));
            pathSegment.remainingBounces--;
            pathSegment.ray.origin = intersect+ 0.0001f * pathSegment.ray.direction;
        }
        if (pathSegment.remainingBounces == 0) {
            pathSegment.color *= 0.f;
        }

    }
}
