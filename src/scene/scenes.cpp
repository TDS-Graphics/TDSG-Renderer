#include "scene/scenes.h"
#include <utility>

void quad(Scene& scene, Vec3f a, Vec3f b, Vec3f c, Vec3f d, int material) {
    scene.addPrimitive({Triangle{{a, b, c}}, material});
    scene.addPrimitive({Triangle{{a, c, d}}, material});
}

void box(Scene& scene, Vec3f size, const Transform& transform, int material) {
    Vec3f p[8];
    for (int i = 0; i < 8; ++i) {
        Vec3f corner;
        if (i & 1) {
            corner.x = 0.5f;
        } else {
            corner.x = -0.5f;
        }

        if (i & 2) {
            corner.y = 1.0f;
        } else {
            corner.y = 0.0f;
        }

        if (i & 4) {
            corner.z = 0.5f;
        } else {
            corner.z = -0.5f;
        }

        p[i] = transform.point(corner * size);
    }

    quad(scene, p[0], p[1], p[5], p[4], material);
    quad(scene, p[2], p[6], p[7], p[3], material);
    quad(scene, p[0], p[2], p[3], p[1], material);
    quad(scene, p[4], p[5], p[7], p[6], material);
    quad(scene, p[0], p[4], p[6], p[2], material);
    quad(scene, p[1], p[3], p[7], p[5], material);
}

SceneSetup MakeCornell() {
    Scene scene;
    int white = scene.addMaterial({MaterialType::Diffuse, Spectrum(0.73f)});
    int red = scene.addMaterial({MaterialType::Diffuse, {0.63f, 0.065f, 0.05f}});
    int green = scene.addMaterial({MaterialType::Diffuse, {0.14f, 0.45f, 0.091f}});
    int light = scene.addMaterial({MaterialType::Diffuse, Spectrum(0), {17, 15, 12}});

    quad(scene, {-1, 0, 1}, {1, 0, 1}, {1, 0, -1}, {-1, 0, -1}, white);
    quad(scene, {-1, 2, -1}, {1, 2, -1}, {1, 2, 1}, {-1, 2, 1}, white);
    quad(scene, {-1, 0, -1}, {1, 0, -1}, {1, 2, -1}, {-1, 2, -1}, white);
    quad(scene, {-1, 0, 1}, {-1, 0, -1}, {-1, 2, -1}, {-1, 2, 1}, red);
    quad(scene, {1, 0, -1}, {1, 0, 1}, {1, 2, 1}, {1, 2, -1}, green);

    quad(scene, {-0.32f, 1.99f, -0.4f}, {0.32f, 1.99f, -0.4f},
                {0.32f, 1.99f, 0.2f}, {-0.32f, 1.99f, 0.2f}, light);

    box(scene, {0.65f, 0.65f, 0.65f}, {{-0.4f, 0, 0.35f}, 1, -18}, white);
    box(scene, {0.65f, 1.3f, 0.65f}, {{0.38f, 0, -0.3f}, 1, 18}, white);

    return {std::move(scene), std::make_unique<PinholeCamera>(
        Vec3f{0, 1, 4}, Vec3f{0, 1, 0}, 39.0f, 512, 512), 512, 512};
}

SceneSetup MakeVeach() {
    Scene scene;
    Vec3f eye{0, 5, 9};
    Vec3f light_center{0, 2.6f, -2.3f};

    int floor = scene.addMaterial({MaterialType::Diffuse, {0.12f, 0.14f, 0.17f}});
    quad(scene, {-5, 0, 4}, {5, 0, 4}, {5, 0, -4}, {-5, 0, -4}, floor);

    const float roughness[] = {0.5f, 0.32f, 0.18f, 0.08f};
    for (int i = 0; i < 4; ++i) {
        int metal = scene.addMaterial({MaterialType::Metal, {0.88f, 0.78f, 0.62f}, Spectrum(0), roughness[i]});
        Vec3f center{0, 0.25f, -0.9f + i * 1.05f};
        Vec3f n = normalize(normalize(eye - center) + normalize(light_center - center));
        Vec3f x{3.8f, 0, 0}, z = normalize(cross({1, 0, 0}, n)) * 0.42f;

        quad(scene, center - x - z, center - x + z, center + x + z, center + x - z, metal);
    }

    const float radii[] = {0.04f, 0.1f, 0.22f, 0.45f};
    for (int i = 0; i < 4; ++i) {
        float r = radii[i];
        int light = scene.addMaterial({MaterialType::Diffuse, Spectrum(0), Spectrum(2 / (r * r))});
        scene.addPrimitive({Sphere{{-2.7f + i * 1.8f, light_center.y, light_center.z}, r}, light});
    }

    return {std::move(scene), std::make_unique<PinholeCamera>(
        eye, Vec3f{0, 0.7f, 0}, 38.0f, 800, 600), 800, 600};
}
