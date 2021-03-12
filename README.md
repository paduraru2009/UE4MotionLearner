# UE4MotionLearner

## Scope:
The purpose of this UE4 plugin is to capture motion data from characters animations, depth and RGB from various camera placed in the world then dump data on disk. It can be used for an automatized deep learning data gathering for animation systems.

## What is able to do:
* Record Characters motion - the skeletal animation SQT for each bone, character and frame.
* Record RGB and depth camera images from fixed / mobile camera that you set in the scene.
* Characters can have different skeletal meshes
* Characters can appear dynamically on the scene (e.g. dissapear/spawn at various time moments)
* Cameras can be either static or dynamic (following your characters). Our system will record things based on their location at each frame.

## How to use:


### Step 0: Clone the repository inside YourProject/Plugins folder, reopen the project and you have a check to activate it.

### Step 1
Grab the MotionLearner actor and drag it to your scene, like in the figure below: ![](https://github.com/paduraru2009/UE4MotionLearner/blob/master/Resources/Help_addComponent.PNG).

### Step 2:
Configure the actor's properties if you wish to modify the camera resolution, frame rate, output paths, etc.
Unless you customize things on your own you should:
* name your characters as capchar_0, capchar_1, .... capchar_N 
* name your cameras as capture_0, capture_1, .... capture_N
One important step is to force the fixed frame rate in the the ProjectSettings to have a fixed frame. 
For example if you want 30 fps set it is an in the figure below: ![](https://github.com/paduraru2009/UE4MotionLearner/blob/master/Resources/Help_fixedFrame.PNG).

### Step 3:
Hit play/start game, do whatever you want in your scene, then stop. You will see output like in the figure below: ![](https://github.com/paduraru2009/UE4MotionLearner/blob/master/Resources/Help_foldersExp.PNG).

Output definition that you will find in **YourProject/Intermediate/outputs** folder:
* motiondata.bin : contains the frames data, SQT for each bone and frame in world space, captured for all characters. Check void AMotionLearnerActor::saveCharactersMotionData() in AMotionLearnerActor.cpp to understand the binary format used.
* capchar_X_bonesmap.json : json for each character containing the mapping, the index in the data above to the bone name.
* Cam_X_FY_rgb.png for each camera index X and frame Y, contains the RGB from the cameras that you set point of view
* Cam_X_FY_depth.png, save as above, but contains the depth data. **Note**: You can't open it, it is a binary file in format: ResolutionHeightxResolutionWidthx1FLOAT, where the float contains for each pixel the distance from camera to the depth on that projected pixel in world space, centimers.
