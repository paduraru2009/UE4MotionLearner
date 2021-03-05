# UE4MotionLearner

## Scope:
The purpose of this UE4 plugin is to capture motion data from character animations and dump data on disk.

## What is able todo:
* Record Characters motion - the skeletal animation SQT for each bone, character and frame
* Record RGB and depth camera images from fixed / mobile camera that you set in the scene.

## How to use:

### Step 1
Grab the MotionLearner actor and drag it to your scene, like in the figure below: ![](http://url/to/img.png).

### Step 2:
Configure the actor's properties if you wish to modify the camera resolution, frame rate, output paths, etc.
One important step is to force the fixed frame rate in the the ProjectSettings to have a fixed frame. 
For example if you want 30 fps set it is an in the figure below: ![](http://url/to/img.png).

### Step 3:
Hit play/start game, do whatever you want in your scene, then stop. You will see output like in the figure below: ![](http://url/to/img.png).

Output definition:
* motiondata.bin : contains the frames 
