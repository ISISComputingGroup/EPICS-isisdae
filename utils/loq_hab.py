# 
# 4 blocks of 12 x 29 pixels (348) but add two dummy elements to round up to 350 
# for start of next block, 1400 pixels in all 
#
# spectra for HAB blocks start at 16387, 16737, 17087, 17437
# these are shifted by 17790 to get to the integrating detecor

n = 17790

# first and last spectrum in map
print("{} {}".format(16387+n, 17437+n+348-1))
# map of specta,x,y
for i in range(0, 348):
    print("{} {} {}".format(i+n+16387, 30 + i % 12, i // 12))    
for i in range(0, 348):
    print("{} {} {}".format(i+n+16737, 41 - i // 12, 30 + i % 12))
for i in range(0, 348):
    print("{} {} {}".format(i+n+17087, i // 12, 11 - i % 12))
for i in range(0, 348):
    print("{} {} {}".format(i+n+17437, 11 - i % 12, 41 - i // 12))
