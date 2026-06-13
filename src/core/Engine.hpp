#pragma once

#include <string>
#include <memory>

struct GLFWwindow;

namespace forge {

class Engine {
public:
    Engine();
    ~Engine();

    bool init(int width, int height, const std::string& title);
    void run();
    void shutdown();

private:
    void handleEvents();
    void update(float deltaTime);
    void render();

    GLFWwindow* window = nullptr;
    
    int windowWidth = 1280;
    int windowHeight = 720;
    std::string windowTitle;
    
    bool isRunning = false;
};

} // namespace forge
