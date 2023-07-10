from gurobipy import Model, GRB, quicksum
import gurobipy as gp
from data_formatting import read_file_to_arrays
import sys

file_names = ["matrixFile0.txt", "matrixFile1.txt", "matrixFile2.txt"]

# tensors
tensor_mappings = []

for file_name in file_names:
    k = read_file_to_arrays(file_name)
    tensor_mappings.append(k)

# tensor id's
n = []
for i in range(len(tensor_mappings[0][0])):
    n.append(i)
# size of tensors
# TODO: these values would be from a generated file
s = []
for i in n:
    s.append(100)

# size of spms
# TODO: these values would be from a generated file
q = [256, 256, 256]


K = len(tensor_mappings)
M = len(tensor_mappings[0])
N = len(tensor_mappings[0][0])

print("K = ", K)
print("M = ", M)
print("N = ", N)


# TODO: loop through the life times to constrain begin times
# make a dict (tensor id , born time)
# if( time the tensor id appears > born time, ignore): else populate it with
# born time
start_time_dict = {}
for k in range(K):
	for m in range(M):
		for n in range(N):
			if tensor_mappings[k][m][n] == 1:
				if n not in start_time_dict:
					start_time_dict[n] = m
				else:
					if m < start_time_dict[n]:
						start_time_dict[n] = m

print("start time dict")
print(start_time_dict)
print("\n")

b = []
for n in range(N):
    b.append(start_time_dict[n])

print(b)

# create matrix that defines whether a tensor has to exist for an operation or not
A = []
# oh my fucking god i want to kms. this is so fucking stupid. im just
# creating a multi dimensional array of 0s bc i dont want to deal with
# numpy types
for m in range(M):
    temp = []
    for n in range(N):
       temp.append(0) 
    A.append(temp)

for k in range(K):
    for m in range(M):
        for n in range(N):
            if tensor_mappings[k][m][n] == 1:
                A[m][n] = 1

model = gp.Model("spm")
X = {}
for k in range(K):
    for m in range(M):
        for n in range(N):
            X[k, m, n] = model.addVar(vtype=GRB.BINARY, name=f"X_{k}_{m}_{n}")
            X[k, m, n].start = tensor_mappings[k][m][n]

y = model.addVars(K, M, N, vtype=GRB.INTEGER, name="y")

# remove absolute value from objective function
for k in range(K):
	for m in range(M-1):
		for n in range(N):
			model.addConstr(y[k, m, n] >= X[k, m+1, n] - X[k, m, n])
			model.addConstr(y[k, m, n] >= -(X[k, m+1, n] - X[k, m, n]))

# a tensor cannot exist on multiple spms's at once and it must exist if we need it
# that operation
for m in range(M):
    for n in range(N):
        if A[m][n] == 1:
            model.addConstr(quicksum(X[k, m, n] for k in
                            range(K)) == 1)

# a tensor cannot exist before it is born
for n in range(N):
    for k in range(K):
        for m in range(M):
            if m < b[n]:
                model.addConstr(X[k, m, n] == 0)

# we can't exceed the spm capacity
for k in range(K):
    for m in range(M):
        model.addConstr(quicksum(X[k, m, n]*s[n] for n in range(N)) <= q[k])

model.setObjective(
	sum(sum(sum(y[k, m, n] for n in range(N)) for m in range(M-1))
			for k in range(K))
    , GRB.MINIMIZE
)

model.optimize()
# Print the optimal values for X
#for k in range(K):
#    for i in range(M):
#        for j in range(N):
#            #print(f"X[{k},{i},{j}] = {X[k, i, j].X}", end=' ')
#            print(f"{X[k, i, j].X}", end=' ')
#        print("")
#    print("\n")
#
#print("\n model.getVars \n")
#for v in model.getVars():
#	print(v.x)

x_optimal = []
spm = []
op = []
for k in range(K):
    for m in range(M):
        for n in range(N):
            if(X[k, m, n].X == -0.0):
                op.append(0)
            else:
            	op.append(X[k, m, n].X)
        spm.append(op)
        op = []
    x_optimal.append(spm)
    spm = []

file_names = ["optimal0.txt", "optimal1.txt", "optimal2.txt"]
for k in range(K):
	file = open(file_names[k], "w")
	for m in range(M):
		for n in range(N):
			file.write('%d ' % x_optimal[k][m][n])
		file.write('\n')
	file.close()
