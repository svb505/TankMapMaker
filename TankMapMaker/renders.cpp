#include "renders.h"
#include <glad/glad.h>

#include "svbmath.h"

constexpr float PI_VALUE = 3.14159265358979323846f;

// Здесь предполагается, что структура tank и переменная tank
// доступны через твои существующие заголовочные файлы.

void DrawCube(float w, float h, float d) {
    glBegin(GL_QUADS);

    // Front
    glVertex3f(-w, -h, d);
    glVertex3f(w, -h, d);
    glVertex3f(w, h, d);
    glVertex3f(-w, h, d);

    // Back
    glVertex3f(-w, -h, -d);
    glVertex3f(-w, h, -d);
    glVertex3f(w, h, -d);
    glVertex3f(w, -h, -d);

    // Left
    glVertex3f(-w, -h, -d);
    glVertex3f(-w, -h, d);
    glVertex3f(-w, h, d);
    glVertex3f(-w, h, -d);

    // Right
    glVertex3f(w, -h, -d);
    glVertex3f(w, h, -d);
    glVertex3f(w, h, d);
    glVertex3f(w, -h, d);

    // Top
    glVertex3f(-w, h, -d);
    glVertex3f(-w, h, d);
    glVertex3f(w, h, d);
    glVertex3f(w, h, -d);

    // Bottom
    glVertex3f(-w, -h, -d);
    glVertex3f(w, -h, -d);
    glVertex3f(w, -h, d);
    glVertex3f(-w, -h, d);

    glEnd();
}
void DrawTankForMapEditor(const svbmath::Vec3& position, float yaw) {
    float gunAngle = 0.0f;
    float turretAngle = 0.0f;
    constexpr float bodyH = 0.5f;
    constexpr float turretSize = 0.5f;

    glPushMatrix();

    // Позиция танка в мире
    glTranslatef(position.x, position.y + bodyH, position.z);

    // Поворот всего танка
    glRotatef(yaw, 0.0f, 1.0f, 0.0f);

    // ================= BODY =================

    glColor3f(0.0f, 0.8f, 0.0f);

    DrawCube(1.0f, bodyH, 1.0f);

    // ================= TURRET =================

    glPushMatrix();

    glTranslatef(0.0f, bodyH + 0.4f, 0.0f);

    glRotatef(
        turretAngle * 180.0f / PI_VALUE + 180.0f,
        0.0f, 1.0f, 0.0f
    );

    glColor3f(0.0f, 0.6f, 0.2f);

    DrawCube(turretSize, turretSize, turretSize);

    // ================= GUN =================

    glPushMatrix();

    glTranslatef(0.0f, 0.0f, turretSize);

    glRotatef(gunAngle, 1.0f, 0.0f, 0.0f);

    constexpr float w = 0.12f;
    constexpr float h = 0.12f;
    constexpr float len = 1.8f;

    glColor3f(0.2f, 0.2f, 0.2f);

    // Передняя грань
    glBegin(GL_QUADS);

    glVertex3f(-w, -h, len);
    glVertex3f(w, -h, len);
    glVertex3f(w, h, len);
    glVertex3f(-w, h, len);

    // Верх
    glVertex3f(-w, h, 0);
    glVertex3f(-w, h, len);
    glVertex3f(w, h, len);
    glVertex3f(w, h, 0);

    // Низ
    glVertex3f(-w, -h, 0);
    glVertex3f(w, -h, 0);
    glVertex3f(w, -h, len);
    glVertex3f(-w, -h, len);

    // Левая сторона
    glVertex3f(-w, -h, 0);
    glVertex3f(-w, -h, len);
    glVertex3f(-w, h, len);
    glVertex3f(-w, h, 0);

    // Правая сторона
    glVertex3f(w, -h, 0);
    glVertex3f(w, h, 0);
    glVertex3f(w, h, len);
    glVertex3f(w, -h, len);

    glEnd();

    glPopMatrix(); // Gun
    glPopMatrix(); // Turret
    glPopMatrix(); // Tank
}
void DrawRadar(const svbmath::Vec3& position, float yaw) {
    float angle = 0.0f;
    
    glPushMatrix();

    // Позиция радара в мире
    glTranslatef(position.x, position.y, position.z);

    // Поворот основания радара
    glRotatef(yaw, 0.0f, 1.0f, 0.0f);

    // ================= BODY =================

    glPushMatrix();

    glScalef(1.2f, 0.25f, 1.2f);
    DrawCube(0.5f, 0.5f, 0.5f);

    glPopMatrix();

    // ================= RACK =================

    glPushMatrix();

    glTranslatef(0.0f, 0.75f, 0.0f);
    glScalef(0.15f, 1.5f, 0.15f);

    DrawCube(0.5f, 0.5f, 0.5f);

    glPopMatrix();

    // ================= ANTENNA =================

    glPushMatrix();

    glTranslatef(0.0f, 1.55f, 0.0f);

    // Вращение антенны
    glRotatef(angle, 0.0f, 1.0f, 0.0f);

    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);

    glScalef(2.5f, 0.08f, 0.35f);

    DrawCube(0.5f, 0.5f, 0.5f);

    glPopMatrix();

    glPopMatrix();
}