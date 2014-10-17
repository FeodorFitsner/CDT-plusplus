#ifndef DELAUNAY_H
#define DELAUNAY_H

// #include <CGAL/Homogeneous_d.h>
// #include <CGAL/gmpxx.h>
#include <CGAL/Cartesian_d.h>
#include <CGAL/Delaunay_d.h>

#include <iostream>

// typedef mpz_class RT;
// typedef CGAL::Homogeneous_d<RT> Kernel;
typedef CGAL::Cartesian_d<double> Kernel;
typedef CGAL::Delaunay_d<Kernel> Delaunay_d;
// typedef Delaunay_d::Point_d Point;
typedef Delaunay_d::Simplex_handle Simplex_handle;
typedef Delaunay_d::Vertex_handle Vertex_handle;
typedef Delaunay_d::Vertex_iterator Vertex_iterator;

class Delaunay : public Delaunay_d
{
public:
  Delaunay(int dimensions) : Delaunay_d(dimensions)
  {
  }

  int CountVertices() {
    // How many vertices do we really have?
    int PointCounter = 0;
    for (Vertex_iterator v = this->vertices_begin(); v != this->vertices_end(); ++v) {
      PointCounter++;
      std::cout << "Point #" << PointCounter << std::endl;
    }
    return PointCounter;
  }
};

#endif
