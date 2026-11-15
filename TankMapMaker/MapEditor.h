#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "models.h"
#include "svbmath.h"

struct MapEditorHooks {
    // Подключи здесь свои функции окружения.
    std::function<void()> drawSky;
    std::function<void(float camX, float camZ)> drawGround;

    // Эти функции вызываются для каждого размещённого танка/радара.
    // Реализуй их через свои существующие функции отрисовки.
    std::function<void(const svbmath::Vec3&, float yaw)> drawTank;
    std::function<void(const svbmath::Vec3&, float yaw)> drawRadar;

    // Если задана, возвращает высоту поверхности в X/Z. Иначе используется Y=0.
    std::function<float(float x, float z)> getGroundHeight;
};

struct MapModelAsset {
    std::string type;      
    std::string displayName;
    std::string objPath;
    float scale = 1.0f;
};

class MapEditor {
public:
    explicit MapEditor(GLFWwindow* window);
    ~MapEditor();

    void SetHooks(MapEditorHooks hooks);
    void AddModelAsset(const MapModelAsset& asset);

    void Initialize();
    void Update(float deltaTime);
    void Render();
    void Shutdown();

    bool SaveJson(const std::string& path) const;
    bool LoadJson(const std::string& path);

private:
    enum class Tool {
        Select,
        Street,
        Tree,
        Lamp,
        Road,
        Fence,
        Trashbin,
        Flowerbed,
        Bench
    };

    struct PlacedObject {
        int id = -1;
        std::string type;
        svbmath::Vec3 position{ 0.0f, 0.0f, 0.0f };
        float yaw = 0.0f;
    };

    struct Road {
        int id = -1;
        svbmath::Vec3 start{ 0.0f, 0.0f, 0.0f };
        svbmath::Vec3 end{ 0.0f, 0.0f, 0.0f };
        float width = 6.0f;
    };

    struct Ray {
        svbmath::Vec3 origin;
        svbmath::Vec3 direction;
    };

    GLFWwindow* window = nullptr;
    MapEditorHooks hooks;

    std::unordered_map<std::string, std::unique_ptr<OBJModel>> models;
    std::unordered_map<std::string, MapModelAsset> assets;

    std::vector<PlacedObject> objects;
    std::vector<Road> roads;

    Tool currentTool = Tool::Select;
    int selectedObject = -1;
    int nextId = 1;

    // Камера редактора. Yaw=0 смотрит вдоль -Z.
    svbmath::Vec3 cameraPosition{ 0.0f, 18.0f, 28.0f };
    float cameraYaw = 0.0f;
    float cameraPitch = -28.0f;
    float cameraFov = 65.0f;
    float moveSpeed = 18.0f;

    bool rightMouseWasDown = false;
    bool leftMouseWasDown = false;
    bool firstMouse = true;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;

    bool roadWaitingForSecondPoint = false;
    svbmath::Vec3 roadFirstPoint{ 0.0f, 0.0f, 0.0f };

    bool initialized = false;

    void DrawScene();
    void DrawGrid(float centerX, float centerZ);
    void DrawFallbackGround();
    void DrawRoad(const Road& road);
    void DrawRoadPreview(const svbmath::Vec3& end);
    void DrawObject(const PlacedObject& object);
    void DrawPreview(const svbmath::Vec3& position);

    void DrawImGui();
    void HandleMouse();
    void HandleKeyboard(float deltaTime);
    void HandlePlacementClick();
    bool GetMouseGroundPoint(svbmath::Vec3& point) const;
    Ray BuildMouseRay() const;
    bool IntersectGround(const Ray& ray, svbmath::Vec3& point) const;

    float GroundHeight(float x, float z) const;
    void ApplyCamera() const;
    void SetTool(Tool tool);
    void DeleteSelected();
    PlacedObject* FindSelectedObject();
    const PlacedObject* FindSelectedObject() const;

    static const char* ToolName(Tool tool);
    static std::string ToolType(Tool tool);

    bool draggingObject = false;
    svbmath::Vec3 dragOffset{ 0.0f, 0.0f, 0.0f };

    // Выделение объектов
    float GetObjectPickRadius(const PlacedObject& object) const;
    bool IntersectObject(const Ray& ray, const PlacedObject& object, float& distance) const;
    int PickObject(const Ray& ray) const;
    void DrawSelection(const PlacedObject& object);

    // Ускорение камеры
    bool cameraBoostEnabled = true;
    float cameraBoostMultiplier = 4.0f;

};

