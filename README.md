# tcxVoronoi

Voronoi fracture for TrussC — split a mesh (or a 2D path) into Voronoi cells for
shatter / destruction effects.

> Status: **Phase 1 (3D convex fracture)**. Concave meshes, 2D paths, extruded
> slabs, and physics integration are planned. See the addon design notes.

## What it does

Given a closed mesh and a set of seed points, `tcxVoronoi` splits the mesh into
one fragment per seed (the Voronoi cell of that seed, intersected with the mesh).
The algorithm cascades half-space plane slices — no external dependency.

```cpp
#include <TrussC.h>
#include <tcxVoronoi.h>
using namespace std;
using namespace tc;
using namespace tcx;

Mesh box = createBox(200.0f);

// simple
FractureResult r = voronoiFracture(box, 30);

// controlled
Voronoi v;
v.addSeed(impactPoint)               // your own points
 .setSeedCount(40)                   // fill the rest...
 .setDistribution(Distribution::Uniform)
 .setRandomSeed(123);
FractureResult r2 = v.fracture(box);

// escape hatch: specify every seed yourself
v.setSeeds(myPoints);

for (auto& cell : r.cells) {
    cell.mesh.draw();                // each fragment is a tc::Mesh
    // cell.seed, cell.centroid, cell.neighbors
}
// r.interfaces : shared faces between adjacent cells (cellA, cellB, point, normal)
// r.neighborsOf(i), r.interfaceBetween(a, b)
```

## Seeds

The final seed set is: your explicit `addSeed()` points, plus auto-generated
points (by the chosen `Distribution`) up to `setSeedCount()`. `setSeeds()`
replaces everything and disables auto-fill. Helpers like `seedsOnSphere()`
return point lists you can feed into `addSeeds()`.

## Example

`example-basic/` fractures a box and animates the pieces exploding apart and
reassembling.

## Physics

`tcxVoronoi` has **no** physics dependency. To drop fragments into a physics
world, include `<tcxVoronoiPhysics.h>` (and add `tcxPhysics` to `addons.make`)
— it adds convenience helpers built on `tcxPhysics`. *(planned)*

## License

MIT. See [LICENSES.md](LICENSES.md).
