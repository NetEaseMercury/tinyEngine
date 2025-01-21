#include "Application.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>

#include "../scene/Scene.h"
#include "../scene/SceneLoader.h"

#include "VulkanRenderer.h"
#include "DebugUtils.h"
#include "Window.h"

GameManage::GameManage()
{
    _window = std::make_unique<Window>(1600, 1200);
    _vulkan = std::make_unique<VulkanInstance>();
    _inputManager = std::make_unique<InputManager>();
    _state = std::make_unique<ApplicationState>();
    _camera = std::make_unique<leoscene::Camera>(glm::vec3(0, -3, 0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0), glm::radians(90.f));
    _ui = std::make_unique<UIManager>();
    sceneLoader = std::make_unique<leoscene::SceneLoader>();
}

GameManage::~GameManage() = default;

int GameManage::init()
{
    if (_window->init()) {
        std::cerr << "Error: Failed to create window." << std::endl;
        return -1;
    }

    _inputManager->init(_window->window, _state.get());
    _inputManager->setCamera(_camera.get());

    try {
        _vulkan->init(_window->window);
    }
    catch (const VulkanRendererException& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Error: Failed to initialize Vulkan instance." << std::endl;
        return -1;
    }

    _renderer = std::make_unique<VulkanRenderer>(_vulkan.get(), _state.get(), _camera.get());

    try {
        _renderer->init();
    }
    catch (const VulkanRendererException& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Error: Failed to initialize Vulkan renderer." << std::endl;
        return -1;
    }

    try
    {
        _ui->setVulkanInstance(_vulkan.get()->getInstance(), nullptr);
        _ui->setPhysicalDevice(_vulkan.get()->getLogicalDevice(), _vulkan.get()->getPhysicalDevice());
        _ui->initIMGUI();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << "Error: Failed to initialize IMGUI." << std::endl;
        return -1;
    }

    return 0;
}

void GameManage::cleanup()
{
    _renderer->cleanup();
    _vulkan->cleanup();
}

int GameManage::loadScene(const std::string& filePath)
{
    leoscene::Scene scene;


    try {
        sceneLoader.get()->loadScene(filePath.c_str(), &scene, _camera.get());
    }
    catch (leoscene::SceneLoaderException e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    try {
        _renderer->loadSceneToDevice(&scene);
    }
    catch (VulkanRendererException e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}

void GameManage::unloadScene()
{
    try {
        sceneLoader.get()->cleanScene();
        _renderer->unloadSceneFromDevice();
    }
    catch (VulkanRendererException e) {
        std::cerr << e.what() << std::endl;
    }
}

int GameManage::start()
{
    try {
        while (_inputManager->processInput()) {
            _renderer->drawFrame();
            _ui->startNewFrame();
            if (_ui.get()->getVulkanRefreshState()) {
                try {
                    _renderer->cleanup();
                    _renderer->init();
                    unloadScene();
                    loadScene(_ui.get()->getScenePath());
                }
                catch (const VulkanRendererException& e) {

                }
                _ui.get()->setVulkanRefreshStatu(false);
            }
        }
    } catch (const VulkanRendererException& e) {
        std::cerr << "Vulkan renderer error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
