from data_formatting import read_file_to_arrays
import gurobipy as gp
from gurobipy import GRB
from gurobipy import quicksum
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

# scratchpad id's
k = [0, 1, 2]

# operations
num_operations = len(tensor_mappings[0])
m = [x for x in range(num_operations)]

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

for k in range(K):
    for m in range(M):
        print(tensor_mappings[k][m])
    print("\n")

try: 
    model = gp.Model("spm")

    y = model.addVars(K, M-1, N, vtype=GRB.INTEGER, name="y")
    z = model.addVars(K, M-1, N, vtype=GRB.INTEGER, name="z")
    
    model.setObjective(z.sum(), GRB.MINIMIZE)
    
    for k in range(K):
        for m in range(M-1):
            for n in range(N):
                model.addConstr(y[k, m, n] == tensor_mappings[k][m+1][n] - 
                                tensor_mappings[k][m][n])
                model.addConstr(y[k, m, n] <= z[k, m, n])
                model.addConstr(-1 * y[k, m, n] <= z[k, m, n])
    
    for i in range(M):
        for j in range(N):
            if A[i][j] == 1:
                model.addConstr(quicksum(tensor_mappings[k][i][j] for k in
                                range(K)) == 1)

    for k in range(K):
        for m in range(M):
            model.addConstr(quicksum(s[n]*tensor_mappings[k][m][n] for n in
                            range(N)) <= q[k])

    model.optimize()
    if model.Status == GRB.INFEASIBLE:
        model.computeIIS()
        model.write('iismodel.ilp')
        print('\nThe following constraints and variables are in the IIS:')
        for c in model.getConstrs():
            if c.IISConstr: print(f'\t{c.constrname}: {model.getRow(c)} {c.Sense} {c.RHS}')

        for v in model.getVars():
            if v.IISLB: print(f'\t{v.varname} ≥ {v.LB}')
            if v.IISUB: print(f'\t{v.varname} ≤ {v.UB}')

    #Relax the bounds and try to make the model feasible
    print('The model is infeasible; relaxing the bounds')
    orignumvars = model.NumVars
    # relaxing only variable bounds
    model.feasRelaxS(0, False, True, False)
    # for relaxing variable bounds and constraint bounds use
    # m.feasRelaxS(0, False, True, True)

    model.optimize()

    status = model.Status
    if status in (GRB.INF_OR_UNBD, GRB.INFEASIBLE, GRB.UNBOUNDED):
        print('The relaxed model cannot be solved \
                because it is infeasible or unbounded')
        sys.exit(1)
    if status != GRB.OPTIMAL:
        print('Optimization was stopped with status %d' % status)
        sys.exit(1)

    # print the values of the artificial variables of the relaxation
    print('\nSlack values:')
    slacks = model.getVars()[orignumvars:]
    for sv in slacks:
        if sv.X > 1e-9:
            print('%s = %g' % (sv.VarName, sv.X))
    print("vars \n")
    for v in model . getVars ():
        print ( '% s % g ' % ( v . VarName , v . X ))
        #print ( ' Obj : % g ' % model . ObjVal )

except gp . GurobiError as e :
    print ( ' Error code ' + str ( e . errno ) + ': ' + str ( e ))
except AttributeError :
    print ( ' Encountered an attribute error ')
