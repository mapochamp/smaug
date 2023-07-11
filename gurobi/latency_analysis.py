from data_formatting import read_file_to_arrays

base_map = ["matrixFile0.txt", "matrixFile1.txt", "matrixFile2.txt"]
optimal_map = ["optimal0.txt", "optimal1.txt", "optimal2.txt"]


# tensors
base_tensor_mappings = []
optimal_tensor_mappings = []

for file_name in base_map:
    k = read_file_to_arrays(file_name)
    base_tensor_mappings.append(k)

for file_name in optimal_map:
    k = read_file_to_arrays(file_name)
    optimal_tensor_mappings.append(k)

# create indicies (dims of matrix) to iterate over
K = len(base_tensor_mappings)  # num of scratchpads
M = len(base_tensor_mappings[0])  # num of operations
N = len(base_tensor_mappings[0][0])  # num of tensors

# get absolute value of differences betwee 0 and 1 (aka store / load)
# we exepct the base map to have a higher number than the optimal
sum_dma_base = 0
for k in range(K):
    for n in range(N):
        for m in range(M-1):
            sum_dma_base += abs(base_tensor_mappings[k][m+1][n] -
                                base_tensor_mappings[k][m][n])

sum_dma_optimal = 0
for k in range(K):
    for n in range(N):
        for m in range(M-1):
            sum_dma_optimal += abs(optimal_tensor_mappings[k][m+1][n] -
                                optimal_tensor_mappings[k][m][n])

print("base dma sum = ", sum_dma_base)
print("optimal dma sum = ", sum_dma_optimal)
#print("fraction = ", (1-(sum_dma_optimal/sum_dma_base))*100 , "%")
