#pragma once
#include "core/ray.h"

class Camera {
public:
    virtual ~Camera() = default;

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual Ray generateRay(Vec2 p_film) const = 0;
};

class PinholeCamera : public Camera {
public:
    PinholeCamera(Vec3f position, Vec3f target, float vertical_fov, int width, int height)
        : position(position), forward(normalize(target - position)),
          right(normalize(cross(forward, {0, 1, 0}))), up(cross(right, forward)),
          image_width(width), image_height(height),
          tan_half_fov(std::tan(radians(vertical_fov) * 0.5f)) {}

    int width() const override { return image_width; }
    int height() const override { return image_height; }

    Ray generateRay(Vec2 p_film) const override {
        float x = (2 * p_film.x / float(image_width) - 1) * (float(image_width) / float(image_height)) * tan_half_fov;
        float y = (1 - 2 * p_film.y / float(image_height)) * tan_half_fov;

        return {position, normalize(forward + x * right + y * up)};
    }

private:
    Vec3f position, forward, right, up;
    int image_width, image_height;
    float tan_half_fov;
};
