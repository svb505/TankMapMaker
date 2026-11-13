#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <ctime>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "MapEditor.h"
#include "texture.h"
#include "renders.h"
#include "environnement.h"
#include "getheight.h"
#include "building.h"

// Подключи свои заголовки, если хочешь использовать существующее окружение:
// #include "environnement.h"
// #include "weather.h"
// #include "tank.h"
// #include "radar.h"

Building build;

void setupGui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "ARIAL.ttf",
        18.0f,
        nullptr,
        io.Fonts->GetGlyphRangesCyrillic()
    );

    if (!font) {
        std::cout << "ERROR: Failed to load res/ARIAL.ttf\n";
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

int main() {
    if (!glfwInit()) {
        std::cerr << "GLFW initialization failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 16);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // Если твой glad/рендер использует compatibility profile, оставь его.
    // Для fixed pipeline (glBegin, glMatrixMode) нужен Compatibility Profile.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 1000, "Map Editor", nullptr, nullptr);
    if (!window) {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD initialization failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    build.CreateBuilding(window);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glClearColor(0.53f, 0.72f, 0.88f, 1.0f);

    setupGui(window);

    // Текстуры должны загружаться после создания OpenGL-контекста.
    LoadAllTextures();

    MapEditor editor(window);

    // Поменяй пути на реальные пути к своим OBJ.
    editor.AddModelAsset({ "street", "Улица", "models/build/rue_2_cotes_complete.obj", 0.6f });
    editor.AddModelAsset({ "tree", "Дерево", "models/trees/tree.obj", 1.0f });
    editor.AddModelAsset({ "lamp", "Фонарный столб", "models/streetlamp/streetlamp.obj", 1.0f });

    MapEditorHooks hooks;

    hooks.drawSky = []() {
         drawSky();
    };
    hooks.drawGround = [](float camX, float camZ) {
         drawGround(camX, camZ);
    };
    hooks.getGroundHeight = [](float x, float z) {
         return getHeight(x, z);
    };

    hooks.drawTank = [](const svbmath::Vec3& pos, float yaw) {
        DrawTankForMapEditor(pos, yaw);
    };
    
    hooks.drawRadar = [](const svbmath::Vec3& pos, float yaw) {
        DrawRadar(pos, yaw);
    };

    editor.SetHooks(std::move(hooks));
    editor.Initialize();

    double previousTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        const double currentTime = glfwGetTime();
        const float deltaTime = static_cast<float>(currentTime - previousTime);
        previousTime = currentTime;

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        editor.Update(deltaTime);
        editor.Render();

        build.DrawBuilding();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    editor.Shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

