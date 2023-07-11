import os
import re

# for each model
# for each size dir
# get base dma sum
# get optimal dma sum
# get dma trasnfer reduction #
# get new dma cycles based on reduce factor
# calculate total acc cycles based on reduce factor
# calculate speed based on reduce factor


models = ["cnn", "elu", "large_elu", "lenet5",
          "lstm", "minerva", "resnet", "vgg"]
#models = ["vgg"]

sizes = ["32kb", "64kb", "128kb", "256kb", "512kb", "1024kb", "2048kb"]

dma_cycles = {
        "vgg" : 1622208,
        "large_elu" : 11122368,
        "resnet" : 1609866,
        "elu" : 1065990,
        "minerva" : 252504,
        "lstm" : 57600,
        "lenet5" : 262566,
        "cnn" : 949542
        }


acc_cycles = {
        "vgg" : 3676823,
        "large_elu" : 24602410,
        "resnet" : 3878749,
        "elu" : 2433286,
        "minerva" : 529860,
        "lstm" : 136278,
        "lenet5" : 643727,
        "cnn" : 2383820
        }

rootdir = '.'

for size in sizes:
    for model in models:
        data = model
        path = "./" + model + "/" + size + "/savings.txt"
        transfer_counts = []
        with open(path, 'r') as file:
            for line in file:
                matches = re.findall(r'\d+', line)
                if matches:
                    transfer_counts.append(int(matches[0]))
        file.close()
        nums = ",".join(map(str, transfer_counts))
        data += "," + nums
        # optimal_dma_sum / base_dma_sum
        new_dma_factor = transfer_counts[1] / transfer_counts[0]
        data += "," + str(1 - new_dma_factor)
        new_dma_count = new_dma_factor * dma_cycles[model]
        new_cycle_count = (acc_cycles[model] - dma_cycles[model]) + new_dma_count
        speedup = (acc_cycles[model]/new_cycle_count) - 1
        data += "," + str(speedup)
        print(data)



# dump data into following format
# model_name , base dma sum, optimal dma sum, dma transfer reduction %, sppedup %
