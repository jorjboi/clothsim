#include "iostream"
#include <nanogui/nanogui.h>

#include "../clothMesh.h"
#include "../clothSimulator.h"
#include "plane.h"

using namespace std;
using namespace CGL;

#define SURFACE_OFFSET 0.0001

void Plane::collide(PointMass &pm) {
  // TODO (Part 3): Handle collisions with planes.
  // Check if the last position and the current position are on opposite
  // sides of the plane.
  double dist_to_last = dot(normal, pm.last_position - point);
  double dist_to_cur = dot(normal, pm.position - point);

  bool last_in_front = dist_to_last >= 0;
  bool cur_in_front = dist_to_cur >= 0;

  // If the point is on the same side as it was before, we don't collide
  if (last_in_front == cur_in_front) {
    return;
  }

  // Bump the point up to the surface of the plane
  Vector3D bumped = pm.position - dist_to_cur * normal; // Final position of bumped point
  Vector3D bump_dir = bumped - pm.last_position; // Direction between bumped point and last position 
  Vector3D tangent = bump_dir - (dot(bump_dir, normal) - SURFACE_OFFSET) * normal; // Remove perpendicular component, only parallel component to the plane
  pm.position = tangent * (1 - friction) + pm.last_position;
}

void Plane::render(GLShader &shader) {
  nanogui::Color color(0.7f, 0.7f, 0.7f, 1.0f);

  Vector3f sPoint(point.x, point.y, point.z);
  Vector3f sNormal(normal.x, normal.y, normal.z);
  Vector3f sParallel(normal.y - normal.z, normal.z - normal.x,
                     normal.x - normal.y);
  sParallel.normalize();
  Vector3f sCross = sNormal.cross(sParallel);

  MatrixXf positions(3, 4);
  MatrixXf normals(3, 4);

  positions.col(0) << sPoint + 2 * (sCross + sParallel);
  positions.col(1) << sPoint + 2 * (sCross - sParallel);
  positions.col(2) << sPoint + 2 * (-sCross + sParallel);
  positions.col(3) << sPoint + 2 * (-sCross - sParallel);

  normals.col(0) << sNormal;
  normals.col(1) << sNormal;
  normals.col(2) << sNormal;
  normals.col(3) << sNormal;

  if (shader.uniform("u_color", false) != -1) {
    shader.setUniform("u_color", color);
  }
  shader.uploadAttrib("in_position", positions);
  if (shader.attrib("in_normal", false) != -1) {
    shader.uploadAttrib("in_normal", normals);
  }

  shader.drawArray(GL_TRIANGLE_STRIP, 0, 4);
}
