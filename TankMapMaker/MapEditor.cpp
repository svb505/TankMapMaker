#define NOMINMAX
#include "MapEditor.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cfloat>
#include <stdexcept>

#include "imgui.h"
#include "nlohmann/json.hpp"
#include "texture.h"

using nlohmann::json;

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;

    svbmath::Vec3 Add(const svbmath::Vec3& a, const svbmath::Vec3& b) {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }
    svbmath::Vec3 Sub(const svbmath::Vec3& a, const svbmath::Vec3& b) {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }
    svbmath::Vec3 Mul(const svbmath::Vec3& a, float k) {
        return { a.x * k, a.y * k, a.z * k };
    }
    float Dot(const svbmath::Vec3& a, const svbmath::Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    svbmath::Vec3 Cross(const svbmath::Vec3& a, const svbmath::Vec3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
    svbmath::Vec3 Normalize(const svbmath::Vec3& v) {
        const float len = std::sqrt(svbmath::Dot(v, v));
        if (len < 0.00001f) return { 0.0f, 0.0f, -1.0f };
        return Mul(v, 1.0f / len);
    }
    float WrapYaw(float yaw) {
        while (yaw < 0.0f) yaw += 360.0f;
        while (yaw >= 360.0f) yaw -= 360.0f;
        return yaw;
    }
    json VecToJson(const svbmath::Vec3& v) {
        return json::array({ v.x, v.y, v.z });
    }
    svbmath::Vec3 JsonToVec(const json& j) {
        if (!j.is_array() || j.size() != 3)
            throw std::runtime_error("Expected a 3-element position array");
        return { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>() };
    }
}

MapEditor::MapEditor(GLFWwindow* window) : window(window) {}

MapEditor::~MapEditor() {
    // GL-ресурсы удаляются в Shutdown(), пока контекст OpenGL ещё существует.
}

void MapEditor::SetHooks(MapEditorHooks value) {
    hooks = std::move(value);
}

void MapEditor::AddModelAsset(const MapModelAsset& asset) {
    assets[asset.type] = asset;
}

void MapEditor::Initialize() {
    if (initialized) return;

    for (const auto& [type, asset] : assets) {
        auto model = std::make_unique<OBJModel>();
        if (!model->LoadOBJ(asset.objPath, asset.scale)) {
            std::cerr << "MapEditor: failed to load model: " << asset.objPath << '\n';
            continue;
        }

        if (type == "lamp") {
            model->LoadTexture("models/streetlamp/texture.png");
        }
        if (type == "tree") {
            model->LoadTextureFromFile("models/tree/texture.png");
        }

        model->CreateVBO();
        models[type] = std::move(model);
    }

    initialized = true;
}

void MapEditor::Shutdown() {
    // Уничтожаем OBJModel до уничтожения GLFW-контекста.
    models.clear();
    initialized = false;
}
void MapEditor::Update(float deltaTime) {
    if (!initialized) return;

    HandleKeyboard(deltaTime);
    HandleMouse();
}
void MapEditor::Render() {
    if (!initialized) return;

    DrawScene();
    DrawImGui();
}
void MapEditor::ApplyCamera() const {
    int width = 1, height = 1;
    glfwGetFramebufferSize(window, &width, &height);
    if (height <= 0) height = 1;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float nearPlane = 0.1f;
    const float top = nearPlane * std::tan(cameraFov * DEG_TO_RAD * 0.5f);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, 1000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-cameraPitch, 1.0f, 0.0f, 0.0f);
    glRotatef(-cameraYaw, 0.0f, 1.0f, 0.0f);
    glTranslatef(-cameraPosition.x, -cameraPosition.y, -cameraPosition.z);
}
float MapEditor::GroundHeight(float x, float z) const {
    return hooks.getGroundHeight ? hooks.getGroundHeight(x, z) : 0.0f;
}
void MapEditor::DrawScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Небо вызывается до установки перспективы, если пользователь подключил свою функцию.
    if (hooks.drawSky) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        hooks.drawSky();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }

    ApplyCamera();

    if (hooks.drawGround)
        hooks.drawGround(cameraPosition.x, cameraPosition.z);
    else
        DrawFallbackGround();

    DrawGrid(cameraPosition.x, cameraPosition.z);

    for (const auto& road : roads)
        DrawRoad(road);

    for (const auto& object : objects)
        DrawObject(object);

    const PlacedObject* selected = FindSelectedObject();

    if (selected) DrawSelection(*selected);

    svbmath::Vec3 mousePoint;
    if (GetMouseGroundPoint(mousePoint)) {
        if (currentTool == Tool::Road && roadWaitingForSecondPoint) {
            DrawRoadPreview(mousePoint);
        }
        else if (currentTool != Tool::Select && currentTool != Tool::Road) {
            DrawPreview(mousePoint);
        }
    }
}
void MapEditor::DrawFallbackGround() {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(0.20f, 0.24f, 0.20f);

    constexpr float extent = 500.0f;
    glBegin(GL_QUADS);
    glVertex3f(cameraPosition.x - extent, 0.0f, cameraPosition.z - extent);
    glVertex3f(cameraPosition.x + extent, 0.0f, cameraPosition.z - extent);
    glVertex3f(cameraPosition.x + extent, 0.0f, cameraPosition.z + extent);
    glVertex3f(cameraPosition.x - extent, 0.0f, cameraPosition.z + extent);
    glEnd();
}
void MapEditor::DrawGrid(float centerX, float centerZ) {
    constexpr float extent = 200.0f;
    constexpr float step = 5.0f;
    const float startX = std::floor(centerX / step) * step;
    const float startZ = std::floor(centerZ / step) * step;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glLineWidth(1.0f);
    glColor4f(0.75f, 0.78f, 0.75f, 0.35f);

    glBegin(GL_LINES);
    for (float x = startX - extent; x <= startX + extent; x += step) {
        glVertex3f(x, GroundHeight(x, startZ) + 0.04f, startZ - extent);
        glVertex3f(x, GroundHeight(x, startZ) + 0.04f, startZ + extent);
    }
    for (float z = startZ - extent; z <= startZ + extent; z += step) {
        glVertex3f(startX - extent, GroundHeight(startX, z) + 0.04f, z);
        glVertex3f(startX + extent, GroundHeight(startX, z) + 0.04f, z);
    }
    glEnd();

    // Оси координат.
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(0.9f, 0.2f, 0.2f);
    glVertex3f(startX - extent, 0.06f, 0.0f);
    glVertex3f(startX + extent, 0.06f, 0.0f);
    glColor3f(0.2f, 0.35f, 1.0f);
    glVertex3f(0.0f, 0.06f, startZ - extent);
    glVertex3f(0.0f, 0.06f, startZ + extent);
    glEnd();
    glLineWidth(1.0f);
}
void MapEditor::DrawRoad(const Road& road) {
    const float dx = road.end.x - road.start.x;
    const float dz = road.end.z - road.start.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length < 0.01f) return;

    const float halfWidth = road.width * 0.5f;
    const float px = -dz / length * halfWidth;
    const float pz = dx / length * halfWidth;

    const float x0 = road.start.x + px;
    const float z0 = road.start.z + pz;
    const float x1 = road.start.x - px;
    const float z1 = road.start.z - pz;
    const float x2 = road.end.x - px;
    const float z2 = road.end.z - pz;
    const float x3 = road.end.x + px;
    const float z3 = road.end.z + pz;

    GLuint roadTexture = 0;
    auto it = allTextures.find("ROAD");
    if (it != allTextures.end()) roadTexture = it->second;

    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_LIGHTING_BIT);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, roadTexture);
    glColor3f(1.0f, 1.0f, 1.0f);

    const float y0 = GroundHeight(road.start.x, road.start.z) + 0.08f;
    const float y1 = GroundHeight(road.end.x, road.end.z) + 0.08f;
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(x0, y0, z0);
    glTexCoord2f(0.0f, length / 10.0f); glVertex3f(x3, y1, z3);
    glTexCoord2f(1.0f, length / 10.0f); glVertex3f(x2, y1, z2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(x1, y0, z1);
    glEnd();

    glPopAttrib();
}
void MapEditor::DrawRoadPreview(const svbmath::Vec3& end) {
    Road preview;
    preview.start = roadFirstPoint;
    preview.end = end;
    preview.width = 6.0f;
    DrawRoad(preview);
}
void MapEditor::DrawObject(const PlacedObject& object) {
    auto it = models.find(object.type);

    if (it == models.end()) {
        std::cout << "MODEL NOT FOUND: "
            << object.type << '\n';
        return;
    }

    if (it->second) {
        it->second->Draw(object.position, object.yaw, 0.0f);
    }
}
void MapEditor::DrawPreview(const svbmath::Vec3& position) {
    PlacedObject preview;
    preview.type = ToolType(currentTool);
    preview.position = position;
    preview.yaw = 0.0f;

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LIGHTING_BIT);
    glDisable(GL_LIGHTING);
    glColor4f(0.65f, 1.0f, 0.65f, 0.65f);
    DrawObject(preview);
    glPopAttrib();
}


MapEditor::Ray MapEditor::BuildMouseRay() const {
    double mouseX = 0.0;
    double mouseY = 0.0;

    glfwGetCursorPos(window, &mouseX, &mouseY);

    int width = 1;
    int height = 1;
    glfwGetWindowSize(window, &width, &height);

    int framebufferWidth = 1;
    int framebufferHeight = 1;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    if (framebufferHeight <= 0) framebufferHeight = 1;

    const float ndcX =
        static_cast<float>(2.0 * mouseX / width - 1.0);

    const float ndcY =
        static_cast<float>(1.0 - 2.0 * mouseY / height);

    // Используем тот же aspect ratio, что и ApplyCamera().
    const float aspect =
        static_cast<float>(framebufferWidth) /
        static_cast<float>(framebufferHeight);

    const float tanHalf =
        std::tan(cameraFov * DEG_TO_RAD * 0.5f);

    const float yaw = cameraYaw * DEG_TO_RAD;
    const float pitch = cameraPitch * DEG_TO_RAD;

    // Базис камеры должен соответствовать ApplyCamera():
    // glRotatef(-pitch, X)
    // glRotatef(-yaw, Y)

    const svbmath::Vec3 forward{
        -std::sin(yaw) * std::cos(pitch),
         std::sin(pitch),
        -std::cos(yaw) * std::cos(pitch)
    };

    const svbmath::Vec3 right{
        std::cos(yaw),
        0.0f,
        -std::sin(yaw)
    };

    const svbmath::Vec3 up =
        svbmath::Normalize(svbmath::Cross(right, forward));

    const svbmath::Vec3 direction = svbmath::Normalize(Add(
        forward,
        Add(
            Mul(right, ndcX * aspect * tanHalf),
            Mul(up, ndcY * tanHalf)
        )
    ));

    return { cameraPosition, direction };
}


bool MapEditor::IntersectGround(const Ray& ray, svbmath::Vec3& point) const {
    if (std::abs(ray.direction.y) < 0.00001f) return false;

    // Сначала пересекаем плоскость Y=0. Для рельефа уточняем высоту итерациями.
    float t = -ray.origin.y / ray.direction.y;
    if (t < 0.0f) return false;

    float x = ray.origin.x + ray.direction.x * t;
    float z = ray.origin.z + ray.direction.z * t;

    for (int i = 0; i < 5; ++i) {
        const float y = GroundHeight(x, z);
        t = (y - ray.origin.y) / ray.direction.y;
        if (t < 0.0f) return false;
        x = ray.origin.x + ray.direction.x * t;
        z = ray.origin.z + ray.direction.z * t;
    }

    point = { x, GroundHeight(x, z), z };
    return true;
}

bool MapEditor::GetMouseGroundPoint(svbmath::Vec3& point) const {
    return IntersectGround(BuildMouseRay(), point);
}

void MapEditor::HandleMouse() {
    const bool rightDown =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (rightDown && !ImGui::GetIO().WantCaptureMouse) {
        double x = 0.0, y = 0.0;
        glfwGetCursorPos(window, &x, &y);

        if (rightMouseWasDown && !firstMouse) {
            cameraYaw += static_cast<float>(x - lastMouseX) * 0.18f;
            cameraPitch -= static_cast<float>(y - lastMouseY) * 0.18f;

            cameraPitch = std::clamp(cameraPitch, -89.0f, 89.0f);
        }

        lastMouseX = x;
        lastMouseY = y;
        firstMouse = false;
    }
    else {
        firstMouse = true;
    }

    rightMouseWasDown = rightDown;

    const bool leftDown =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    const bool mouseCaptured = ImGui::GetIO().WantCaptureMouse;

    // Нажатие ЛКМ.
    if (leftDown && !leftMouseWasDown && !mouseCaptured) {
        HandlePlacementClick();
    }

    // Перемещение выбранного объекта.
    if (leftDown &&
        leftMouseWasDown &&
        draggingObject &&
        currentTool == Tool::Select &&
        !mouseCaptured) {

        PlacedObject* selected = FindSelectedObject();

        if (selected) {
            svbmath::Vec3 mousePoint;

            if (GetMouseGroundPoint(mousePoint)) {
                selected->position.x = mousePoint.x + dragOffset.x;
                selected->position.z = mousePoint.z + dragOffset.z;
            }
        }
    }

    // Отпустили ЛКМ — прекращаем перемещение.
    if (!leftDown) {
        draggingObject = false;
    }

    leftMouseWasDown = leftDown;
}


void MapEditor::HandlePlacementClick() {
    if (currentTool == Tool::Select) {
        selectedObject = PickObject(BuildMouseRay());
        draggingObject = false;

        if (selectedObject != -1) {
            PlacedObject* selected = FindSelectedObject();

            svbmath::Vec3 mousePoint;

            if (selected && GetMouseGroundPoint(mousePoint)) {
                // Сохраняем смещение между объектом и курсором.
                dragOffset.x = selected->position.x - mousePoint.x;
                dragOffset.y = 0.0f;
                dragOffset.z = selected->position.z - mousePoint.z;

                draggingObject = true;
            }

            std::cout << "Selected object ID: "
                << selectedObject << '\n';
        }
        else {
            std::cout << "No object selected\n";
        }

        return;
    }

    svbmath::Vec3 point;

    if (!GetMouseGroundPoint(point))
        return;

    if (currentTool == Tool::Road) {
        if (!roadWaitingForSecondPoint) {
            roadFirstPoint = point;
            roadWaitingForSecondPoint = true;
        }
        else {
            Road road;

            road.id = nextId++;
            road.start = roadFirstPoint;
            road.end = point;
            road.width = 6.0f;

            const float dx = road.end.x - road.start.x;
            const float dz = road.end.z - road.start.z;

            if (std::sqrt(dx * dx + dz * dz) > 0.5f)
                roads.push_back(road);

            roadWaitingForSecondPoint = false;
        }

        return;
    }

    PlacedObject object;

    object.id = nextId++;
    object.type = ToolType(currentTool);
    object.position = point;
    object.yaw = 0.0f;

    objects.push_back(object);
    selectedObject = object.id;
}

void MapEditor::DrawSelection(const PlacedObject& object) {
    glPushAttrib(
        GL_ENABLE_BIT |
        GL_CURRENT_BIT |
        GL_LINE_BIT |
        GL_LIGHTING_BIT
    );

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glColor3f(1.0f, 0.85f, 0.1f);
    glLineWidth(3.0f);

    const float radius = GetObjectPickRadius(object);
    const float y = GroundHeight(
        object.position.x,
        object.position.z
    ) + 0.12f;

    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < 64; ++i) {
        const float angle = 2.0f * 3.14159265358979323846f * i / 64.0f;

        glVertex3f(
            object.position.x + std::cos(angle) * radius,
            y,
            object.position.z + std::sin(angle) * radius
        );
    }

    glEnd();

    glPopAttrib();
}


float MapEditor::GetObjectPickRadius(const PlacedObject& object) const {
    if (object.type == "street") return 4.0f;
    if (object.type == "tree") return 2.0f;
    if (object.type == "lamp") return 1.2f;
    if (object.type == "tank") return 2.5f;
    if (object.type == "radar") return 2.5f;

    return 2.0f;
}


bool MapEditor::IntersectObject(
    const Ray& ray,
    const PlacedObject& object,
    float& distance
) const {
    // Половины размеров приблизительных Bounding Box.
    svbmath::Vec3 halfSize{ 2.0f, 3.0f, 2.0f };

    if (object.type == "street")
        halfSize = { 6.0f, 5.0f, 6.0f };

    else if (object.type == "tree")
        halfSize = { 3.0f, 7.0f, 3.0f };

    else if (object.type == "lamp")
        halfSize = { 1.5f, 5.0f, 1.5f };

    else if (object.type == "tank")
        halfSize = { 3.5f, 2.5f, 4.5f };

    else if (object.type == "radar")
        halfSize = { 3.5f, 5.0f, 3.5f };

    // Переводим луч в локальные координаты объекта.
    // Это позволяет учитывать yaw.
    const float angle = -object.yaw * DEG_TO_RAD;

    const float c = std::cos(angle);
    const float s = std::sin(angle);

    const svbmath::Vec3 relative = Sub(
        ray.origin,
        object.position
    );

    const svbmath::Vec3 origin{
        c * relative.x + s * relative.z,
        relative.y,
        -s * relative.x + c * relative.z
    };

    const svbmath::Vec3 direction{
        c * ray.direction.x + s * ray.direction.z,
        ray.direction.y,
        -s * ray.direction.x + c * ray.direction.z
    };

    float tMin = 0.0f;
    float tMax = FLT_MAX;

    auto checkAxis = [&](float o, float d, float half) -> bool {
        if (std::abs(d) < 0.000001f)
            return o >= -half && o <= half;

        float t1 = (-half - o) / d;
        float t2 = (half - o) / d;

        if (t1 > t2)
            std::swap(t1, t2);

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);

        return tMin <= tMax;
        };

    if (!checkAxis(origin.x, direction.x, halfSize.x))
        return false;

    if (!checkAxis(origin.y, direction.y, halfSize.y))
        return false;

    if (!checkAxis(origin.z, direction.z, halfSize.z))
        return false;

    if (tMax < 0.0f)
        return false;

    distance = tMin;
    return true;
}


int MapEditor::PickObject(const Ray& ray) const {
    int closestId = -1;
    float closestDistance = FLT_MAX;

    for (const auto& object : objects) {
        float distance = 0.0f;

        if (IntersectObject(ray, object, distance)) {
            if (distance < closestDistance) {
                closestDistance = distance;
                closestId = object.id;
            }
        }
    }

    return closestId;
}

void MapEditor::HandleKeyboard(float deltaTime) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    const float yaw = cameraYaw * DEG_TO_RAD;

    const svbmath::Vec3 forward{ -std::sin(yaw), 0.0f, -std::cos(yaw) };
    const svbmath::Vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};

    svbmath::Vec3 movement{ 0.0f, 0.0f, 0.0f };
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement = Add(movement, forward);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement = Sub(movement, forward);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement = Add(movement, right);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement = Sub(movement, right);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) movement.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) movement.y -= 1.0f;

    float currentSpeed = moveSpeed;

    if (cameraBoostEnabled &&
        (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)) {
        currentSpeed *= cameraBoostMultiplier;
    }

    if (svbmath::Dot(movement, movement) > 0.0f) {
        cameraPosition = Add(
            cameraPosition,
            Mul(
                svbmath::Normalize(movement),
                currentSpeed * deltaTime
            )
        );
    }

    PlacedObject* selected = FindSelectedObject();
    if (selected) {
        const float rotationSpeed = 90.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            selected->yaw = WrapYaw(selected->yaw - rotationSpeed);
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            selected->yaw = WrapYaw(selected->yaw + rotationSpeed);
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        roadWaitingForSecondPoint = false;
}

void MapEditor::SetTool(Tool tool) {
    currentTool = tool;
    if (tool != Tool::Road) roadWaitingForSecondPoint = false;
}

const char* MapEditor::ToolName(Tool tool) {
    switch (tool) {
    case Tool::Select: return "Выбор";
    case Tool::Street: return "Улица";
    case Tool::Tree: return "Дерево";
    case Tool::Lamp: return "Фонарный столб";
    case Tool::Road: return "Дорога";
    default: return "Неизвестно";
    }
}

std::string MapEditor::ToolType(Tool tool) {
    switch (tool) {
    case Tool::Street: return "street";
    case Tool::Tree: return "tree";
    case Tool::Lamp: return "lamp";
    default: return {};
    }
}

MapEditor::PlacedObject* MapEditor::FindSelectedObject() {
    for (auto& object : objects)
        if (object.id == selectedObject) return &object;
    return nullptr;
}

const MapEditor::PlacedObject* MapEditor::FindSelectedObject() const {
    for (const auto& object : objects)
        if (object.id == selectedObject) return &object;
    return nullptr;
}

void MapEditor::DeleteSelected() {
    objects.erase(
        std::remove_if(objects.begin(), objects.end(),
            [this](const PlacedObject& o) { return o.id == selectedObject; }),
        objects.end());
    selectedObject = -1;
}

void MapEditor::DrawImGui() {
    ImGui::Begin("Map Editor");

    ImGui::Text("Инструменты");
    if (ImGui::Button("Выбор")) SetTool(Tool::Select);
    ImGui::SameLine();

    if (ImGui::Button("Улица")) SetTool(Tool::Street);
    ImGui::SameLine();
    if (ImGui::Button("Дерево")) SetTool(Tool::Tree);
    ImGui::SameLine();
    if (ImGui::Button("Фонарный столб")) SetTool(Tool::Lamp);
    ImGui::SameLine();
    if (ImGui::Button("Дорога")) SetTool(Tool::Road);

    ImGui::Separator();
    ImGui::Text("Активный инструмент: %s", ToolName(currentTool));
    if (currentTool == Tool::Road) {
        ImGui::TextWrapped("%s", roadWaitingForSecondPoint
            ? "Укажи вторую точку дороги."
            : "Укажи первую точку дороги.");
        if (ImGui::Button("Отменить построение дороги"))
            roadWaitingForSecondPoint = false;
    }
    ImGui::Text("Управление: WASD — камера, ПКМ + мышь — обзор, Space/Ctrl — высота");
    ImGui::Text("Стрелки влево/вправо — поворот выбранного объекта");

    ImGui::Separator();
    ImGui::Text("Настройки камеры");

    ImGui::SliderFloat(
        "Скорость камеры",
        &moveSpeed,
        1.0f,
        100.0f
    );

    ImGui::Checkbox(
        "Ускорение камеры (Shift)",
        &cameraBoostEnabled
    );

    if (cameraBoostEnabled) {
        ImGui::SliderFloat(
            "Множитель ускорения",
            &cameraBoostMultiplier,
            1.0f,
            10.0f,
            "%.1fx"
        );
    }

    ImGui::Separator();
    ImGui::Text("Объекты на карте: %d", static_cast<int>(objects.size()));
    if (ImGui::BeginListBox("##objects", ImVec2(-FLT_MIN, 150.0f))) {
        for (const auto& object : objects) {
            std::string displayName = object.type;

            if (object.type == "street") displayName = "Улица";
            else if (object.type == "tree") displayName = "Дерево";
            else if (object.type == "lamp") displayName = "Фонарный столб";
            else if (object.type == "tank") displayName = "Танк";
            else if (object.type == "radar") displayName = "Радар";

            const std::string label = displayName + "  #" + std::to_string(object.id);
            if (ImGui::Selectable(label.c_str(), selectedObject == object.id))
                selectedObject = object.id;
        }
        ImGui::EndListBox();
    }

    if (PlacedObject* selected = FindSelectedObject()) {
        ImGui::Separator();
        ImGui::Text("Выбран объект #%d (%s)", selected->id, selected->type.c_str());
        float position[3] = {
            selected->position.x, selected->position.y, selected->position.z
        };
        if (ImGui::DragFloat3("Позиция", position, 0.1f)) {
            selected->position = { position[0], position[1], position[2] };
        }
        ImGui::SliderFloat("Yaw", &selected->yaw, 0.0f, 360.0f, "%.1f");
        if (ImGui::Button("Удалить выбранный объект"))
            DeleteSelected();
    }

    ImGui::Separator();
    static char filePath[260] = "maps/map.json";
    ImGui::InputText("JSON-файл", filePath, sizeof(filePath));
    if (ImGui::Button("Сохранить карту")) {
        if (SaveJson(filePath))
            std::cout << "Map saved: " << filePath << '\n';
        else
            std::cerr << "Cannot save map: " << filePath << '\n';
    }
    ImGui::SameLine();
    if (ImGui::Button("Загрузить карту")) {
        if (!LoadJson(filePath))
            std::cerr << "Cannot load map: " << filePath << '\n';
    }

    ImGui::Text("Дорог: %d", static_cast<int>(roads.size()));
    ImGui::End();
}

bool MapEditor::SaveJson(const std::string& path) const {
    try {
        json root;
        root["version"] = 1;
        root["objects"] = json::array();
        root["roads"] = json::array();

        for (const auto& object : objects) {
            root["objects"].push_back({
                { "id", object.id },
                { "type", object.type },
                { "position", VecToJson(object.position) },
                { "yaw", object.yaw }
                });
        }

        for (const auto& road : roads) {
            root["roads"].push_back({
                { "id", road.id },
                { "start", VecToJson(road.start) },
                { "end", VecToJson(road.end) },
                { "width", road.width }
                });
        }

        const std::filesystem::path output(path);
        if (output.has_parent_path())
            std::filesystem::create_directories(output.parent_path());

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << root.dump(4);
        return static_cast<bool>(file);
    }
    catch (const std::exception& e) {
        std::cerr << "SaveJson error: " << e.what() << '\n';
        return false;
    }
}

bool MapEditor::LoadJson(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        json root;
        file >> root;
        if (!root.is_object() || root.value("version", 0) != 1) return false;

        std::vector<PlacedObject> loadedObjects;
        std::vector<Road> loadedRoads;
        int maxId = 0;

        for (const auto& item : root.at("objects")) {
            PlacedObject object;
            object.id = item.at("id").get<int>();
            object.type = item.at("type").get<std::string>();
            object.position = JsonToVec(item.at("position"));
            object.yaw = WrapYaw(item.at("yaw").get<float>());
            loadedObjects.push_back(object);
            maxId = std::max(maxId, object.id);
        }

        for (const auto& item : root.at("roads")) {
            Road road;
            road.id = item.at("id").get<int>();
            road.start = JsonToVec(item.at("start"));
            road.end = JsonToVec(item.at("end"));
            road.width = item.value("width", 6.0f);
            loadedRoads.push_back(road);
            maxId = std::max(maxId, road.id);
        }

        objects = std::move(loadedObjects);
        roads = std::move(loadedRoads);
        selectedObject = -1;
        nextId = maxId + 1;
        roadWaitingForSecondPoint = false;
        SetTool(Tool::Select);

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "LoadJson error: " << e.what() << '\n';
        return false;
    }
}

