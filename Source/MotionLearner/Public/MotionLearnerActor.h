// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MotionLearnerActor.generated.h"

// Decides either to capture at the same time color and depth or two instantiate two separate scene capture ocmponents
// From experiments, it is much faster to capture as ARGB8 instead of float for all channels
// , so decided to go with two separate components and render targets per camera

//#define USE_COLORANDDEPTH

UCLASS()
class MOTIONLEARNER_API AMotionLearnerActor : public AActor
{
	GENERATED_BODY()

	struct CameraCaptureInfo
	{
		ACameraActor* cameraActor  = nullptr;
		USceneCaptureComponent2D* sceneCaptureComponent2D[2] = {nullptr, nullptr};
		UTextureRenderTarget2D* renderTarget2D[2] = {nullptr, nullptr};
		FString actorName;
		int index=999;

		bool debug_savedOneFrame = false; // True if this camera saved at least one frame
	};

	struct CharacterCaptureInfo
	{
		FString actorName;
		int actorIndex = -1;
		TArray<FName> boneNames;
		class USkinnedMeshComponent* skinnedMeshComponent = nullptr; // mesh ref of the captured actor

		struct BoneFrameData
		{
			// world space
			FVector scale;
			FQuat quat;
			FVector loc;			
		};

		// Data for each bone on a single frame
		struct FrameData
		{
			int actorIndex = -1;
			
			// Data for each bone in a frame in the given order of boneNames above
			TArray<BoneFrameData> characterFrameData;
			void preallocate(const int numBones)
			{
				characterFrameData.Reserve(numBones);
			}
		};
	};

	// The captured data per frame for all characters
	struct AllCharactersFrameData
	{
		int frameid = -1;
		TArray<CharacterCaptureInfo::FrameData> capturedData;

		void preallocate(const int numChars)
		{
			capturedData.Reserve(numChars);
		}
	};

	// The structure containing the captured data by frame, then by actor suitable for streaming
	TArray<AllCharactersFrameData> m_framesCharactersCapture;
	
public:	
	// Sets default values for this actor's properties
	AMotionLearnerActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void initSystems();
	void augmentAllCameraActors();
	void registerCharacterActors();
	void captureFrameCharacters();
	void solveCamerasOutput();
	void saveCharactersMotionData();

	// The array of all camera actors and their attached scene capture component
	TArray<CameraCaptureInfo> m_allCameraActors;
	TArray<CharacterCaptureInfo> m_allCharactersToCapture;

	// Attaching to the camera actor a scene capture component with the given actor parent and name
	// Returning back the scene capture component and the texture render target
	void prepareCaptureComponentForCamera(ACameraActor* cameraActor, const FName sceneCompName, CameraCaptureInfo& outCameraInfo);

	int m_frameCounter = -1;
	int m_isInitialized = false;

	// Some temp buffers
#ifdef USE_COLORANDDEPTH
	TArray<FFloat16Color> SurfData;
	renderTargetResource->ReadFloat16Pixels(SurfData);
#else
	TArray<FLinearColor> m_SurfDataTemp;
#endif
	TArray<float> m_SurfDataDepth_Temp;
			
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// The prefix of camera actors that will be used to do scene capture
	UPROPERTY(EditAnywhere, Category="Customizations")
	FString TargetCameraPrefix = "capture";


	// The suffix of the scene capture components  that will be attached to cameras and  used to do scene capture
	UPROPERTY(EditAnywhere, Category = "Customizations")
	FString TargetSceneCompSuffix = "scenecomp";

		// The prefix of camera actors that will be used to do scene capture
	UPROPERTY(EditAnywhere, Category="Customizations")
	FString TargetCharacterPrefix = "capchar";

	FString FolderOutputPath = "";

	
	UPROPERTY(EditAnywhere, Category = "Customizations")
	int ResolutionWidth = 512;

	UPROPERTY(EditAnywhere, Category = "Customizations")
	int ResolutionHeight = 512;

	// Framerate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customizations")
	int FrameRate = 30;

	// Should update ?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customizations")
	bool EnabledUpdated = true;

	// Should save images captured o ndisk ? 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customizations")
	bool EnabledSavingOnDisk = true;

	// Should save each frame ?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customizations")
	bool EnabledSavingEachFrame = false;
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customizations")
	FString MotionBinaryFileName = TEXT("motiondata.bin");
};
