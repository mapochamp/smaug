def read_file_to_arrays(file_path):
    # Initialize an empty list to store the lines
    multi_dim_array = []
    
    # Open the file
    with open(file_path, 'r') as file:
        # Read each line in the file
        for line in file:
            # Remove the newline character, split by spaces, and convert to integers
            num_array = [int(num) for num in line.strip().split(' ')]
            # Append this array to the multi-dimensional array
            multi_dim_array.append(num_array)
    
    # Return the multi-dimensional array after the file is closed
    return multi_dim_array

def write_array_to_file(file_path):
   pass 
