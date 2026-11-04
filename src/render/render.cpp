#include "render/render.h"
#include <atomic>
#include <cstdio>
#include <mutex>

// 多线程分块渲染
void Render(const Scene& scene, const Camera& camera, Film& film,
            const Integrator& integrator, const RenderSettings& settings) {
    constexpr int tile_size = 16;
    int width = camera.width();
    int height = camera.height();
    int tiles_x = (width + tile_size - 1) / tile_size;
    int tiles_y = (height + tile_size - 1) / tile_size;
    int total_tiles = tiles_x * tiles_y;

    std::atomic<int> next_tile{0};
    std::atomic<int> completed_tiles{0};
    std::mutex progress_mutex;
    int last_percent = 0;

    const auto printProgress = [](float percentage) {
        constexpr char progress_bar[] = "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";
        constexpr int progress_width = 60;
        int val = int(percentage * 100);
        int lpad = int(percentage * progress_width);
        int rpad = progress_width - lpad;

        std::printf("\r%3d%% [%.*s%*s]", val, lpad, progress_bar, rpad, "");
        std::fflush(stdout);
    };

    printProgress(0);

    auto worker = [&] {
        for (;;) {
            int tile = next_tile.fetch_add(1);
            if (tile >= total_tiles) {
                break;
            }

            int x0 = (tile % tiles_x) * tile_size;
            int y0 = (tile / tiles_x) * tile_size;

            for (int y = y0; y < std::min(y0 + tile_size, height); ++y) {
                for (int x = x0; x < std::min(x0 + tile_size, width); ++x) {
                    for (int s = 0; s < settings.spp; ++s) {
                        Sampler sampler = Sampler::forSample(settings.seed, uint64_t(y) * width + x, s);
                        Vec2 p_film = Vec2{float(x), float(y)} + sampler.next2D();
                        Ray ray = camera.generateRay(p_film);

                        Spectrum color = integrator.Li(ray, scene, sampler);
                        film.addSample(x, y, color);
                    }
                }
            }

            int completed = completed_tiles.fetch_add(1, std::memory_order_relaxed) + 1;
            int percent = int(100LL * completed / total_tiles);

            std::lock_guard<std::mutex> lock(progress_mutex);
            if (percent > last_percent) {
                printProgress(float(completed) / total_tiles);
                last_percent = percent;
            }
        }
    };

    std::vector<std::thread> workers;
    int thread_count = std::min(settings.threads, total_tiles);
    for (int i = 1; i < thread_count; ++i) {
        workers.emplace_back(worker);
    }

    worker();

    for (auto& thread : workers) {
        thread.join();
    }

    {
        std::lock_guard<std::mutex> lock(progress_mutex);
        if (last_percent < 100) {
            printProgress(1);
        }

        std::printf("\n");
    }
}
