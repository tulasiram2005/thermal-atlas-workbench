# Thermal Atlas

Thermal Atlas is an offline engineering workbench for exploring heat conduction and numerical verification. Its C++17 solver computes temperatures along a rod and compares four finite-element meshes against an analytical solution. A local React interface plots temperatures and convergence, displays STL geometry and thermal CSV data, and exports computed results.

The workbench also compares serial and parallel batches, checks backpropagation gradients using finite differences, and simulates device connection and recovery. A Windows launcher starts the local service and opens the interface in the browser.
