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

sizes = ["32kb", "64kb", "128kb", "256kb", "512kb", "1024kb", "2048kb"]

import os
rootdir = '.'

for subdir, dirs, files in os.walk(rootdir):
    for file in files:
        print(os.path.join(subdir, file))


# dump data into following format

# model name
# base dma sum
# optimal dma sum
# dma fraction post reduce
# speedup
