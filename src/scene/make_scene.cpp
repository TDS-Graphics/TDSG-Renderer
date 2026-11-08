#include "scene/make_scene.h"
#include "scene/obj_loader.h"
#include "light/point_light.h"
#include <array>
#include <utility>

namespace {

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

int cornellBox(Scene& scene, bool green_left = false) {
    int white = scene.addMaterial({MaterialType::Diffuse, Spectrum(0.73f)});
    int red = scene.addMaterial({MaterialType::Diffuse, {0.63f, 0.065f, 0.05f}});
    int green = scene.addMaterial({MaterialType::Diffuse, {0.14f, 0.45f, 0.091f}});
    int light = scene.addMaterial({MaterialType::Diffuse, Spectrum(0), {17, 15, 12}});

    quad(scene, {-1, 0, 1}, {1, 0, 1}, {1, 0, -1}, {-1, 0, -1}, white);
    quad(scene, {-1, 2, -1}, {1, 2, -1}, {1, 2, 1}, {-1, 2, 1}, white);
    quad(scene, {-1, 0, -1}, {1, 0, -1}, {1, 2, -1}, {-1, 2, -1}, white);
    quad(scene, {-1, 0, 1}, {-1, 0, -1}, {-1, 2, -1}, {-1, 2, 1}, green_left ? green : red);
    quad(scene, {1, 0, -1}, {1, 0, 1}, {1, 2, 1}, {1, 2, -1}, green_left ? red : green);

    quad(scene, {-0.32f, 1.99f, -0.4f}, {0.32f, 1.99f, -0.4f},
                {0.32f, 1.99f, 0.2f}, {-0.32f, 1.99f, 0.2f}, light);

    return white;
}

} // namespace

SceneSetup MakeCornell() {
    Scene scene;
    int white = cornellBox(scene);
    scene.addPrimitive({Sphere{{-0.43f, 0.43f, 0.3f}, 0.43f}, white});
    scene.addPrimitive({Sphere{{0.4f, 0.55f, -0.35f}, 0.55f}, white});

    return {std::move(scene), std::make_unique<PinholeCamera>(
        Vec3f{0, 1, 4}, Vec3f{0, 1, 0}, 39.0f, 512, 512)};
}

SceneSetup MakeCornellGlass() {
    Scene scene;
    int white = cornellBox(scene);
    int glass = scene.addMaterial({MaterialType::Glass, Spectrum(1), Spectrum(0), 0.3f, 1.5f});

    box(scene, {0.65f, 0.65f, 0.65f}, {{-0.4f, 0, 0.35f}, 1, -18}, white);
    scene.addPrimitive({Sphere{{0.38f, 0.6f, -0.3f}, 0.6f}, glass});

    return {std::move(scene), std::make_unique<PinholeCamera>(
        Vec3f{0, 1, 4}, Vec3f{0, 1, 0}, 39.0f, 512, 512)};
}

SceneSetup MakeRoughness() {
    Scene scene;
    int white = scene.addMaterial({MaterialType::Diffuse, Spectrum(0.73f)});
    int green = scene.addMaterial({MaterialType::Diffuse, {0.14f, 0.45f, 0.091f}});
    int red = scene.addMaterial({MaterialType::Diffuse, {0.63f, 0.065f, 0.05f}});
    int light = scene.addMaterial({MaterialType::Diffuse, Spectrum(0), Spectrum(3)});

    quad(scene, {-3.4f, 0, 4}, {3.4f, 0, 4}, {3.4f, 0, -1.5f}, {-3.4f, 0, -1.5f}, white);
    quad(scene, {-3.4f, 2.4f, -1.5f}, {3.4f, 2.4f, -1.5f}, {3.4f, 2.4f, 4}, {-3.4f, 2.4f, 4}, white);
    quad(scene, {-3.4f, 0, -1.5f}, {3.4f, 0, -1.5f}, {3.4f, 2.4f, -1.5f}, {-3.4f, 2.4f, -1.5f}, white);
    quad(scene, {-3.4f, 0, 4}, {-3.4f, 0, -1.5f}, {-3.4f, 2.4f, -1.5f}, {-3.4f, 2.4f, 4}, green);
    quad(scene, {3.4f, 0, -1.5f}, {3.4f, 0, 4}, {3.4f, 2.4f, 4}, {3.4f, 2.4f, -1.5f}, red);
    quad(scene, {-2.8f, 2.39f, -1.15f}, {2.8f, 2.39f, -1.15f},
                {2.8f, 2.39f, 1.2f}, {-2.8f, 2.39f, 1.2f}, light);

    const float roughness[] = {0.02f, 0.12f, 0.25f, 0.4f, 0.65f, 1.0f};
    for (int i = 0; i < 6; ++i) {
        int metal = scene.addMaterial({MaterialType::Metal, Spectrum(0.9f), Spectrum(0), roughness[i]});
        scene.addPrimitive({Sphere{{-2.65f + 1.06f * i, 0.48f, 0}, 0.48f}, metal});
    }

    return {std::move(scene), std::make_unique<PinholeCamera>(
        Vec3f{2.8f, 1.25f, 8.5f}, Vec3f{0, 1, 0}, 20.0f, 1000, 440)};
}

SceneSetup MakeAreaLight() {
    Scene scene;
    int floor = scene.addMaterial({MaterialType::Diffuse, Spectrum(0.55f)});
    int occluder = scene.addMaterial({MaterialType::Diffuse, Spectrum(0.35f)});
    int light = scene.addMaterial({MaterialType::Diffuse, Spectrum(0), Spectrum(6)});

    quad(scene, {-2.2f, 0, 1.65f}, {2.2f, 0, 1.65f}, {2.2f, 0, -1.65f}, {-2.2f, 0, -1.65f}, floor);
    quad(scene, {-2.18f, 0.02f, 0.65f}, {-2.18f, 0.02f, -1.5f},
                {-2.18f, 2.2f, -1.5f}, {-2.18f, 2.2f, 0.65f}, light);
    box(scene, {0.3f, 1.9f, 0.3f}, {{0, 0, -0.25f}, 1, -12}, occluder);

    return {std::move(scene), std::make_unique<PinholeCamera>(
        Vec3f{5, 3.2f, 6}, Vec3f{0, 0.65f, 0}, 29.0f, 800, 450)};
}

SceneSetup MakeLucy() {
    Scene scene;
    int white = cornellBox(scene, true);
    int glass = scene.addMaterial({MaterialType::Glass, Spectrum(1), Spectrum(0), 0.3f, 1.5f});

    // The bundled OBJ is centered in X/Z, rests at Y=0, and has unit height.
    const auto model = std::filesystem::path(TDSG_ASSET_DIR) / "lucy.obj";
    LoadObj(scene, model, {{-0.48f, 0, -0.32f}, 1.45f, 0}, white);
    LoadObj(scene, model, {{0.42f, 0, 0.15f}, 1.72f, 0}, glass);

    return {std::move(scene), std::make_unique<PinholeCamera>(
        Vec3f{0, 1, 4}, Vec3f{0, 1, 0}, 39.0f, 512, 512)};
}

SceneSetup MakeWhitted() {
    Scene scene;
    scene.setBackground({0.08f, 0.3f, 0.65f});

    // Reference: https://alphapixeldev.com/we-solve-your-difficult-problems-forward-porting-with-legacy-media-and-code-part-5-animation-and-texturing/
    int red = scene.addMaterial({MaterialType::Diffuse, {0.85f, 0.025f, 0.015f}});
    int yellow = scene.addMaterial({MaterialType::Diffuse, {0.9f, 0.8f, 0.035f}});
    int glass = scene.addMaterial({MaterialType::Glass, Spectrum(1), Spectrum(0), 0.3f, 1.5f});
    int mirror = scene.addMaterial({MaterialType::Mirror, Spectrum(0.95f)});

    constexpr int tiles = 16;
    constexpr float tile_size = 0.65f;
    for (int z = 0; z < tiles; ++z) {
        for (int x = 0; x < tiles; ++x) {
            float x0 = (x - tiles / 2) * tile_size;
            float z0 = (z - tiles / 2) * tile_size;
            int material = (x + z) % 2 == 0 ? yellow : red;
            quad(scene, {x0, 0, z0}, {x0, 0, z0 + tile_size},
                        {x0 + tile_size, 0, z0 + tile_size}, {x0 + tile_size, 0, z0}, material);
        }
    }

    scene.addPrimitive({Sphere{{-1.05f, 1.75f, -0.4f}, 1.25f}, glass});
    scene.addPrimitive({Sphere{{1.1f, 0.8f, 0.55f}, 0.8f}, mirror});
    scene.addLight(std::make_shared<PointLight>(Vec3f{-3, 6, 4}, Spectrum(220)));
    //scene.addLight(std::make_shared<PointLight>(Vec3f{4, 6, -3}, Spectrum(25)));

    return {std::move(scene), std::make_unique<PinholeCamera>(
        Vec3f{0, 3.2f, 9}, Vec3f{0, 1.1f, 0}, 39.0f, 800, 600)};
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
        eye, Vec3f{0, 0.7f, 0}, 38.0f, 800, 600)};
}

std::optional<SceneSetup> makeScene(std::string_view name) {
    // TODO 自定义场景描述文件
    using Factory = SceneSetup (*)();
    static constexpr std::array<std::pair<std::string_view, Factory>, 7> factories{{
        {"cornell", MakeCornell},
        {"cornell_glass", MakeCornellGlass},
        {"roughness_test", MakeRoughness},
        {"area_light", MakeAreaLight},
        {"lucy", MakeLucy},
        {"whitted", MakeWhitted},
        {"veach", MakeVeach},
    }};

    for (const auto& [factory_name, createScene] : factories) {
        if (factory_name == name) {
            return createScene();
        }
    }

    return std::nullopt;
}

