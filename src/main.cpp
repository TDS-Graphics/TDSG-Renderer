/*
    TDSG Renderer, an educational physically-based light transport simulator.

    本渲染器的初衷是用作 TDS 游戏开发协会图形组（TDS Graphics）的教学，它在未来还将作为一个实验
    被学校图形学课程所采纳！

    TDSG Renderer is Copyright (c) 2026 by Tianyi Bai (@BrownieCake-). MIT License.

    thirdparty:
    - tinyobjloader is Copyright (c) 2012-Present, Syoyo Fujita and many contributors. MIT License.
    - tinybvh is Copyright (c) 2024 Jacco Bikker. MIT License.
    - pbrt v3 is Copyright (c) 1998-2015, Matt Pharr, Greg Humphreys, and Wenzel Jakob. BSD license.
    - pbrt v4 is Copyright (c) 1998-2020, Matt Pharr, Greg Humphreys, and Wenzel Jakob. Apache-2.0.

    The MIT License (MIT)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "render/render.h"
#include "scene/obj_loader.h"
#include "scene/make_scene.h"

int main(int argc, char** argv) {
    // 默认参数
    std::string scene_name = "cornell";
    std::string integrator_name = "normal";
    std::string output = "render.ppm";
    float exposure = 0;
    Transform obj_transform;
    std::vector<std::string> obj_paths;
    RenderSettings settings;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto getValue = [&]() { return argv[++i]; };

        if (arg == "--help") {
            std::cout << "TDSG Renderer v0.0.3\n"
                        << "Copyright (c) 2026 by BrownieCake- @ TDS, under MIT License.\n"
                        << "Usage:\n"
                        << "  --scene whitted|cornell|cornell_glass|roughness_test|area_light|lucy|veach\n"
                        << "                            Default: cornell\n"
                        << "  --spp N --depth N         Default: 4, 8\n"
                        << "  --threads N --seed N      Default: hardware threads, 1\n"
                        << "  --integrator path|nee|mis|whitted|normal  Default: normal\n"
                        << "  --output image.ppm|.pfm   Default: render.ppm\n"
                        << "  --exposure EV             PPM exposure in stops\n"
                        << "  --obj model.obj           Add an OBJ, with MTL constants\n"
                        << "  --obj-scale S             Uniform scale\n"
                        << "  --obj-rotate-y DEGREES\n"
                        << "  --obj-translate X Y Z\n";
            return 0;
        } else if (arg == "--scene") {
            scene_name = getValue();
        } else if (arg == "--spp") {
            settings.spp = std::atoi(getValue());
        } else if (arg == "--depth") {
            settings.max_depth = std::atoi(getValue());
        } else if (arg == "--threads") {
            settings.threads = std::atoi(getValue());
        } else if (arg == "--seed") {
            settings.seed = std::strtoull(getValue(), nullptr, 10);
        } else if (arg == "--output") {
            output = getValue();
        } else if (arg == "--exposure") {
            exposure = std::strtof(getValue(), nullptr);
        } else if (arg == "--obj") {
            obj_paths.emplace_back(getValue());
        } else if (arg == "--obj-scale") {
            obj_transform.scale = std::strtof(getValue(), nullptr);
        } else if (arg == "--obj-rotate-y") {
            obj_transform.rotation_y = std::strtof(getValue(), nullptr);
        } else if (arg == "--obj-translate") {
            obj_transform.translation.x = std::strtof(getValue(), nullptr);
            obj_transform.translation.y = std::strtof(getValue(), nullptr);
            obj_transform.translation.z = std::strtof(getValue(), nullptr);
        } else if (arg == "--integrator") {
            integrator_name = getValue();
        }
    }

    // 场景
    auto setup = makeScene(scene_name);
    if (!setup) {
        std::cout << "Unknown scene: " << scene_name << '\n';
        return -1;
    }

    for (const std::string& path : obj_paths) {
        LoadObj(setup->scene, path, obj_transform);
    }

    setup->scene.Build();

    // integrator
    auto integrator = makeIntegrator(integrator_name, settings.max_depth);
    if (!integrator) {
        std::cout << "Unknown integrator: " << integrator_name << '\n';
        return -1;
    }

    // 相机胶片
    Film film(setup->camera->width(), setup->camera->height());

    std::cout << scene_name << ": " << film.width() << 'x' << film.height()
                << ", " << settings.spp << " spp, " << setup->scene.primitiveCount() << " primitives\n";


    // Render, START!!!!!        
    auto start = std::chrono::steady_clock::now();

    Render(setup->scene, *setup->camera, film, *integrator, settings);
    film.Save(output, exposure);

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Saved " << output << " (" << elapsed.count() << " s)\n";

    return 0;
}
