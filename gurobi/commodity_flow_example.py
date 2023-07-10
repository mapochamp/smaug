import itertools
import matplotlib.pyplot as plt
import gurobipy as gp
from gurobipy import GRB

solveLPOnly=True
# Some toy data: 2 commodities with a fractional LP basic optimum
Supplies= {
# node i: [supply commodity[1] ... supply commodity[K]],
1: [1., 0.],
2: [0., -1.],
3: [0., 0.],
4: [0., 0.],
5: [0., 0.],
6: [0., 0.],
7: [0., 1.],
8: [-1., 0.]}
CapacityCosts = {
# arc (i,j): [capacity, cost commodity[1] ... cost commodity[K]],
(1,2): [1., 1, 1],
(1,3): [1., 1, 1],
(2,5): [1., 1, 1],
(3,4): [1., 1, 1],
(4,1): [1., 1, 1],
(4,7): [1., 1, 1],
(5,6): [1., 1, 1],
(6,2): [1., 1, 1],
(6,8): [1., 1, 1],
(7,3): [1., 1, 1],
(7,8): [1., 1, 1],
(8,5): [1., 1, 1]}

Nodes=list(Supplies.keys()) # get node list from supply data
print("Nodes")
print(Nodes)
print(" \n")
K=len(Supplies[Nodes[0]]) # get number of commodities from supply data
print("K")
print(K)
print(" \n")
Commods=list(range(1,K+1)) # name the commodities 1,2,...,K
print("Commods")
print(Commods)
print(" \n")
Arcs=list(CapacityCosts.keys()) # get arc list from Capacity/Cost data
print("Arcs")
print(Arcs)
print(" \n")
ArcsCrossCommods=list(itertools.product(Arcs,Commods))
print("ArcsCrossCommods")
print(ArcsCrossCommods)
print(" \n")
