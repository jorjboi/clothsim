#include <iostream>
#include <math.h>
#include <random>
#include <vector>

#include "cloth.h"
#include "collision/plane.h"
#include "collision/sphere.h"

using namespace std;

Cloth::Cloth(double width, double height, int num_width_points,
             int num_height_points, float thickness) {
  this->width = width;
  this->height = height;
  this->num_width_points = num_width_points;
  this->num_height_points = num_height_points;
  this->thickness = thickness;

  buildGrid();
  buildClothMesh();
}

Cloth::~Cloth() {
  point_masses.clear();
  springs.clear();

  if (clothMesh) {
    delete clothMesh;
  }
}

void Cloth::buildGrid() {
   // Build point masses
  for (int y = 0; y < num_height_points; y++) {
    for (int x = 0; x < num_width_points; x++) {
      Vector3D pos;
      double rand_offset;
      if (orientation == VERTICAL) {
        rand_offset = rand() * 1.0f / RAND_MAX / 1000.0f;
        pos = Vector3D((double)(x * width) / num_width_points,
                       (double)(y * height) / num_height_points, rand_offset);
      } else {
        pos = Vector3D((double)(x * width) / num_width_points, 1.0,
                       (double)(y * height) / num_height_points);
      }

      // Check if it is within pinned vector of this cloth
      bool is_pinned = false;
      for (int i = 0; i < pinned.size(); i++) {
        vector<int> test = {x, y};
        if (pinned[i] == test) {
          is_pinned = true;
        }
      }

      // Build point pass, add to point mass vector of this cloth
      PointMass pm(pos, is_pinned);
      point_masses.push_back(pm);
    }
  }

  // Build springs
  for (int y = 0; y < num_height_points; y++) {
    for (int x = 0; x < num_width_points; x++) {
      int idx = y * num_width_points + x; // Row-major order
      PointMass *pm = &point_masses[idx];

      // Structural constraints
      if (x != 0) {
        Spring s(pm, pm - 1, STRUCTURAL); // to the left
        springs.push_back(s);
      }

      if (y != 0) {
        Spring s(pm, pm - num_width_points, STRUCTURAL); // on top
        springs.push_back(s);
      }

      // Shear constraints
      if (x > 0 && y > 0) {
        Spring s(pm, pm - num_width_points - 1, SHEARING); // upper left
        springs.push_back(s);
      }

      if (x < num_width_points - 1 && y > 0) {
        Spring s(pm, pm - num_width_points + 1, SHEARING); // upper right
        springs.push_back(s);
      }

      // Bending constraints
      if (y > 1) {
        Spring s(pm, pm - 2 * num_width_points, BENDING); // left two
        springs.push_back(s);
      }

      if (x < num_width_points - 2) {
        Spring s(pm, pm + 2, BENDING); // up two
        springs.push_back(s);
      }
    }
  }
}

void Cloth::simulate(double frames_per_sec, double simulation_steps, ClothParameters *cp,
                     vector<Vector3D> external_accelerations,
                     vector<CollisionObject *> *collision_objects) {
  double mass = width * height * cp->density / num_width_points / num_height_points;
  double delta_t = 1.0f / frames_per_sec / simulation_steps;

  // TODO (Part 2): Compute total force acting on each point mass.
  
  // Sum all external forces
  Vector3D external_forces(0, 0, 0);
  for (auto accel : external_accelerations) {
    external_forces += accel * mass;
  }
  // Reset all existing forces on point masses
  for (auto &pm : point_masses) {
    pm.forces.x = external_forces.x;
    pm.forces.y = external_forces.y;
    pm.forces.z = external_forces.z;
  }

  // Compute and accumulate spring correction forces into each point mass
  for (auto &s : springs) {
    if ((s.spring_type == STRUCTURAL && !cp->enable_structural_constraints) ||
        (s.spring_type == SHEARING && !cp->enable_shearing_constraints) ||
        (s.spring_type == BENDING && !cp->enable_bending_constraints)) {
      continue;
    }

    // Use Hooke's law to calculate the force on either point mass
    Vector3D diff = s.pm_b->position - s.pm_a->position;
    Vector3D normalized_diff = diff.unit(); //  unit direction vector

    double correction = cp->ks * (diff.norm() - s.rest_length); // F = kx 

    if (s.spring_type == BENDING) {
        correction *= 0.2f;
    }

    Vector3D spring_force = normalized_diff * correction; // Force vector (directional)
    s.pm_a->forces += spring_force;
    s.pm_b->forces -= spring_force;
  }


  // TODO (Part 2): Use Verlet integration to compute new point mass positions
  for (auto &pm : point_masses) {
    if (!pm.pinned) {
      Vector3D acceleration = pm.forces / mass;
      Vector3D new_position =
          pm.position +
          (1 - cp->damping / 100) * (pm.position - pm.last_position) +
          acceleration * delta_t * delta_t;

      pm.last_position = pm.position;
      pm.position = new_position;
    }
  }

  // TODO (Part 4): Handle self-collisions.
  build_spatial_map();
  for (PointMass &pm : point_masses) {
    self_collide(pm, simulation_steps);
  }


  // TODO (Part 3): Handle collisions with other primitives.
  for (PointMass &pm : point_masses) {
    for (CollisionObject *co : *collision_objects) {
      co->collide(pm);
    }
  }

  // TODO (Part 2): Constrain the changes to be such that the spring does not change
  // in length more than 10% per timestep [Provot 1995].
    for (auto &s : springs) {
      Vector3D pb_to_pa = s.pm_a->position - s.pm_b->position;
      float distance = pb_to_pa.norm();

      float maximum_deformation = s.rest_length * 1.1;
      if (distance > maximum_deformation) {
        Vector3D correction = pb_to_pa.unit() * (distance - maximum_deformation);

        if (s.pm_a->pinned && !s.pm_b->pinned) {
          s.pm_b->position += correction;
        } else if (!s.pm_a->pinned && s.pm_b->pinned) {
          s.pm_a->position -= correction;
        } else if (!s.pm_a->pinned && !s.pm_b->pinned) {
          s.pm_a->position -= correction * 0.5;
          s.pm_b->position += correction * 0.5;
        }  
      }
    }
}

void Cloth::build_spatial_map() {
  for (const auto &entry : map) {
    delete(entry.second);
  }
  map.clear();

  // TODO (Part 4): Build a spatial map out of all of the point masses.
  for (PointMass &pm : point_masses) {
    float bucket = hash_position(pm.position);

    if (map[bucket] != nullptr) {
      map[bucket]->push_back(&pm);
    } else {
      vector<PointMass *> *pms = new vector<PointMass *>();
      pms->push_back(&pm);
      map[bucket] = pms;
    }
  }
}

void Cloth::self_collide(PointMass &pm, double simulation_steps) {
  // TODO (Part 4): Handle self-collision for a given point mass.
    float bucket = hash_position(pm.position);
    // Get vector of point masses occupying the same "bucket" (3D space) 
    vector<PointMass *> *pms = map[bucket];
    if (!pms)
      return;
    
    Vector3D correction(0, 0, 0); // To append to pm's current position
    int n = 0;

    // Look at every neighbor of this point mass
    for (PointMass *temp_pm : *pms) {
      if (temp_pm == &pm)
        continue; // Don't collide point mass with itself
      Vector3D dir = pm.position - temp_pm->position; // Move it away from the temp pm
      double dist = dir.norm();
      if (dist <= 2 * thickness) { // If it's closer than twice the thickness of our cloth
        double overlap = 2 * thickness - dist; 
        correction += overlap * dir.unit(); // Move back overlap amount so it's 2 thickness apart
        n+=1;
      }    
    }
    if (n > 0) {
      pm.position = pm.position + correction / simulation_steps / (float) n; // the average of all of these pairwise correction vectors, scaled down by simulation_steps
    }
    
}

float Cloth::hash_position(Vector3D pos) {
  // TODO (Part 4): Hash a 3D position into a unique float identifier that represents membership in some 3D box volume.
  int p = 223; // random prime number

  float w = (width / num_width_points) * 3;
  float h = (height / num_height_points) * 3;
  float t = max(w, h);

  float x = pos.x - fmod(pos.x, w);
  float y = pos.y - fmod(pos.y, h);
  float z = pos.z - fmod(pos.z, t);
  
  //unsigned int hash = (xi * 92837111) ^ (yi * 689287499) ^ (zi * 283923481);
  // return std::abs(static_cast<int>(hash)); // Ensure the hash is positive
  return p * p * x + p * y + z;
}

///////////////////////////////////////////////////////
/// YOU DO NOT NEED TO REFER TO ANY CODE BELOW THIS ///
///////////////////////////////////////////////////////

void Cloth::reset() {
  PointMass *pm = &point_masses[0];
  for (int i = 0; i < point_masses.size(); i++) {
    pm->position = pm->start_position;
    pm->last_position = pm->start_position;
    pm++;
  }
}

void Cloth::buildClothMesh() {
  if (point_masses.size() == 0) return;

  ClothMesh *clothMesh = new ClothMesh();
  vector<Triangle *> triangles;

  // Create vector of triangles
  for (int y = 0; y < num_height_points - 1; y++) {
    for (int x = 0; x < num_width_points - 1; x++) {
      PointMass *pm = &point_masses[y * num_width_points + x];
      // Get neighboring point masses:
      /*                      *
       * pm_A -------- pm_B   *
       *             /        *
       *  |         /   |     *
       *  |        /    |     *
       *  |       /     |     *
       *  |      /      |     *
       *  |     /       |     *
       *  |    /        |     *
       *      /               *
       * pm_C -------- pm_D   *
       *                      *
       */
      
      float u_min = x;
      u_min /= num_width_points - 1;
      float u_max = x + 1;
      u_max /= num_width_points - 1;
      float v_min = y;
      v_min /= num_height_points - 1;
      float v_max = y + 1;
      v_max /= num_height_points - 1;
      
      PointMass *pm_A = pm                       ;
      PointMass *pm_B = pm                    + 1;
      PointMass *pm_C = pm + num_width_points    ;
      PointMass *pm_D = pm + num_width_points + 1;
      
      Vector3D uv_A = Vector3D(u_min, v_min, 0);
      Vector3D uv_B = Vector3D(u_max, v_min, 0);
      Vector3D uv_C = Vector3D(u_min, v_max, 0);
      Vector3D uv_D = Vector3D(u_max, v_max, 0);
      
      
      // Both triangles defined by vertices in counter-clockwise orientation
      triangles.push_back(new Triangle(pm_A, pm_C, pm_B, 
                                       uv_A, uv_C, uv_B));
      triangles.push_back(new Triangle(pm_B, pm_C, pm_D, 
                                       uv_B, uv_C, uv_D));
    }
  }

  // For each triangle in row-order, create 3 edges and 3 internal halfedges
  for (int i = 0; i < triangles.size(); i++) {
    Triangle *t = triangles[i];

    // Allocate new halfedges on heap
    Halfedge *h1 = new Halfedge();
    Halfedge *h2 = new Halfedge();
    Halfedge *h3 = new Halfedge();

    // Allocate new edges on heap
    Edge *e1 = new Edge();
    Edge *e2 = new Edge();
    Edge *e3 = new Edge();

    // Assign a halfedge pointer to the triangle
    t->halfedge = h1;

    // Assign halfedge pointers to point masses
    t->pm1->halfedge = h1;
    t->pm2->halfedge = h2;
    t->pm3->halfedge = h3;

    // Update all halfedge pointers
    h1->edge = e1;
    h1->next = h2;
    h1->pm = t->pm1;
    h1->triangle = t;

    h2->edge = e2;
    h2->next = h3;
    h2->pm = t->pm2;
    h2->triangle = t;

    h3->edge = e3;
    h3->next = h1;
    h3->pm = t->pm3;
    h3->triangle = t;
  }

  // Go back through the cloth mesh and link triangles together using halfedge
  // twin pointers

  // Convenient variables for math
  int num_height_tris = (num_height_points - 1) * 2;
  int num_width_tris = (num_width_points - 1) * 2;

  bool topLeft = true;
  for (int i = 0; i < triangles.size(); i++) {
    Triangle *t = triangles[i];

    if (topLeft) {
      // Get left triangle, if it exists
      if (i % num_width_tris != 0) { // Not a left-most triangle
        Triangle *temp = triangles[i - 1];
        t->pm1->halfedge->twin = temp->pm3->halfedge;
      } else {
        t->pm1->halfedge->twin = nullptr;
      }

      // Get triangle above, if it exists
      if (i >= num_width_tris) { // Not a top-most triangle
        Triangle *temp = triangles[i - num_width_tris + 1];
        t->pm3->halfedge->twin = temp->pm2->halfedge;
      } else {
        t->pm3->halfedge->twin = nullptr;
      }

      // Get triangle to bottom right; guaranteed to exist
      Triangle *temp = triangles[i + 1];
      t->pm2->halfedge->twin = temp->pm1->halfedge;
    } else {
      // Get right triangle, if it exists
      if (i % num_width_tris != num_width_tris - 1) { // Not a right-most triangle
        Triangle *temp = triangles[i + 1];
        t->pm3->halfedge->twin = temp->pm1->halfedge;
      } else {
        t->pm3->halfedge->twin = nullptr;
      }

      // Get triangle below, if it exists
      if (i + num_width_tris - 1 < 1.0f * num_width_tris * num_height_tris / 2.0f) { // Not a bottom-most triangle
        Triangle *temp = triangles[i + num_width_tris - 1];
        t->pm2->halfedge->twin = temp->pm3->halfedge;
      } else {
        t->pm2->halfedge->twin = nullptr;
      }

      // Get triangle to top left; guaranteed to exist
      Triangle *temp = triangles[i - 1];
      t->pm1->halfedge->twin = temp->pm2->halfedge;
    }

    topLeft = !topLeft;
  }

  clothMesh->triangles = triangles;
  this->clothMesh = clothMesh;
}
