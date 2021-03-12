import matplotlib.pyplot as plt
import numpy as np
import math 

frame_index = 1
camera_index = 0
folder_path = 'F:/WORK/UnrealProjects/AnimationsTest/Intermediate/outputs'
for frame_index in range(100, 200):
	rgb = []
	depth_map = []
	for camera_index in range(1):
		rgb.append(plt.imread(f'{folder_path}/Cam_{camera_index:02d}_F{frame_index:05d}_rgb.png'))
		depth_image_path = f'{folder_path}/Cam_{camera_index:02d}_F{frame_index:05d}_depth.png'
		with open(depth_image_path, 'rb') as f:
				depth_bytes = f.read()
				imgBytes = np.frombuffer(depth_bytes, dtype=np.float32, count=-1)
				imgResWidth = int(math.sqrt(len(imgBytes)))
				assert imgResWidth*imgResWidth == len(imgBytes), "Image is not square size"
				imgBytes = imgBytes.reshape(imgResWidth, imgResWidth)

				depth_map.append(imgBytes)
				#print(f"MinVal={depth_map[-1].min()}, MaxVal={depth_map[-1].max()} Avg={depth_map[-1].mean()}")
		
_ = plt.figure(figsize=(20, 20))
plt.imshow(np.concatenate(rgb, axis=1))
plt.show()

_ = plt.figure(figsize=(20, 20))
plt.imshow(np.concatenate(depth_map, axis=1), cmap=plt.get_cmap('hot'), interpolation='nearest',
               vmin=0, vmax=65536)
plt.show()

