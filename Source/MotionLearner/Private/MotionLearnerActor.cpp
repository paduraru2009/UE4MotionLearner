// Fill out your copyright notice in the Description page of Project Settings.


#include "MotionLearnerActor.h"


#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SceneCaptureComponent2D.h"
//#include "MotionLearner/Public/MotionLearnerActor.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Character.h"
#include "Serialization/JsonWriter.h"

// Sets default values
AMotionLearnerActor::AMotionLearnerActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	if (EnabledUpdated)
		PrimaryActorTick.bCanEverTick = true;

	FolderOutputPath = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("outputs"));
	
}

// Called when the game starts or when spawned
void AMotionLearnerActor::BeginPlay()
{
	Super::BeginPlay();

	SetActorTickInterval(1.0/FrameRate);
	m_frameCounter = -1;
	m_isInitialized = false;
}

void AMotionLearnerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	saveCharactersMotionData();

	m_isInitialized = false;
}

void AMotionLearnerActor::initSystems()
{
	//  Allocate temp buffers
	const int numPixelsExpected = ResolutionWidth * ResolutionHeight;
	m_SurfDataTemp.Reserve(numPixelsExpected);
	m_SurfDataDepth_Temp.Reserve(numPixelsExpected);
	m_SurfDataDepth_Temp.AddUninitialized(numPixelsExpected);
	int allocSz = m_SurfDataDepth_Temp.GetAllocatedSize();
	allocSz = allocSz;

	m_isInitialized = true;
}

void AMotionLearnerActor::saveCharactersMotionData()
{
	// Step 0: Save captured frames data for each individual character. Dynamic sets of characters or frame skipping is also possible !
	/* Save format:
	NumFramesCaptured - Integer 4 bytes
	NumFramesCaptured x [ frame data],
		where frame data is composed of:
		[frame_id - integer 4 bytes | characters_data_frame_id],
			where characters_data_frame_id is composed of:
			[ character_0_data, character_1_data, ....] -> list of all filtered characters that appear in the scene at frame_id
				where character_i_data is composed of:
				  [character_i_id - numberic 4 bytes | character_id_numBonesOf(character_i_data) - numberic 4 bytes,
						bone_0_values, bone_1_values, ...., bones_numBonesOf(character_i_data)-1_values]]
							where each bone_i_values is represented by 10 floats (4 bytes) describing: location (3) quaternion (4) scale (3)
	*/

	// Create the file archiever first
	FString motionDataFileName = MotionBinaryFileName;
	FString TotalFileName = FPaths::Combine(FolderOutputPath, motionDataFileName);
	FText PathError;
	FPaths::ValidatePath(TotalFileName, &PathError);
	FArchive* Ar_p = IFileManager::Get().CreateFileWriter(*TotalFileName);
	//"FString::Printf(TEXT("%s_%5d"), TargetCharacterPrefix, allCharsFrameData.frameid);"
					
	if (!Ar_p)
		return;

	// Write number of frames
	int32 numChars = m_framesCharactersCapture.Num();
	FArchive& Ar = *Ar_p;
	Ar << numChars;
	
	for (int frameIndex = 0; frameIndex < numChars; frameIndex++)
	{
		const AllCharactersFrameData& allCharsFrameData = m_framesCharactersCapture[frameIndex]; // Should be const but Ar is stupid !!

		// Write frame id - NOT NECESSARLY FRAME INDEX - E.g. game could be paused
		Ar << (int32)allCharsFrameData.frameid;

		const int numCharactersInFrame = allCharsFrameData.capturedData.Num();
		for (int characterIndex = 0; characterIndex < numCharactersInFrame; characterIndex++)
		{
			const CharacterCaptureInfo::FrameData& characterFrameData = allCharsFrameData.capturedData[characterIndex];

			// Write character index - NOT NECESSARILY characterIndex because some could be destroyed or spawned at runtime dynamically
			Ar << (int32) characterFrameData.actorIndex;

			const int numBonesOfCharacter = characterFrameData.characterFrameData.Num();
			Ar << (int32) numBonesOfCharacter;
			
			for (int boneIndex = 0; boneIndex < numBonesOfCharacter; boneIndex++)
			{
				const CharacterCaptureInfo::BoneFrameData& boneFrameData = characterFrameData.characterFrameData[boneIndex];
				const FVector& loc = boneFrameData.loc;
				const FQuat& quat = boneFrameData.quat;
				const FVector& scale = boneFrameData.scale;

				Ar << (float) loc.X << (float) loc.Y << (float) loc.Z;
				Ar << (float) quat.X << (float) quat.Y << (float) quat.Z << (float)quat.W;
				Ar << (float) scale.X << (float) scale.Y << (float) scale.Z;
			}
		}
	}

	delete Ar_p;


	// Step 1: save the map bones for each character
	for (int characterIndex = 0; characterIndex < m_allCharactersToCapture.Num(); characterIndex++)
	{
		TArray<FName>& boneNamesMap = m_allCharactersToCapture[characterIndex].boneNames;

		TSharedPtr<FJsonObject> characterBonesMap = MakeShareable(new FJsonObject);
		for (int boneIndex = 0; boneIndex < boneNamesMap.Num(); boneIndex++)
		{			
			characterBonesMap->SetStringField(FString::FromInt(boneIndex), boneNamesMap[boneIndex].ToString());
		}
		

		FString filename = FString::Printf(TEXT("%s_bonesmap.json"), *m_allCharactersToCapture[characterIndex].actorName);
		FString fullName = FPaths::Combine(FolderOutputPath, filename); FPaths::ProjectIntermediateDir();
		FPaths::ValidatePath(fullName, &PathError);
		FArchive* mapB_p = IFileManager::Get().CreateFileWriter(*fullName);
		if (!mapB_p)
			return;		
		FArchive& mapB = *mapB_p;

		FString OutputString;
		TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(characterBonesMap.ToSharedRef(), Writer);

		mapB <<OutputString;
		delete mapB_p;

		// We will use this FileManager to deal with the file.
		IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
		
	   // We use the LoadFileToString to load the file into
	   if(FFileHelper::SaveStringToFile(OutputString,*fullName))
	   {
	      UE_LOG(LogTemp, Warning, TEXT("FileManipulation: Sucsesfuly Written: \"%s\" to the text file"),*OutputString);
	   }
	   else
	   {
	      UE_LOG(LogTemp, Warning, TEXT("FileManipulation: Failed to write FString to file."));
	   }
	}	
}


// Called every frame
void AMotionLearnerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//check(DeltaSeconds >= 1.0f / m_frameRate);
	const float currentTickRate = GetActorTickInterval();
	check(FMath::Abs(currentTickRate - 1.0 / FrameRate) <= 0.00001f);
	m_frameCounter++;

	// We initialize on the first tick because that is the moment when every object in the scene was instantiated .. hope so
	if (!m_isInitialized)
	{
		initSystems();
		registerCharacterActors();
		augmentAllCameraActors();
	}
	else // Can't capture in the first frame because render targets are not created yet :(
	{	
		captureFrameCharacters();
		solveCamerasOutput();
	}
}

void AMotionLearnerActor::registerCharacterActors()
{
	// Mine all character actors that have the name with a givel filter
	TArray<AActor*> allActorsFound;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), allActorsFound);

	m_allCharactersToCapture.Empty(allActorsFound.Num());

	for (AActor* actor : allActorsFound)
	{
		ACharacter* characterActor = Cast<ACharacter, AActor>(actor);
		if (!characterActor)
			continue;
		
		const FString actorName = characterActor->GetName();

		//Filtering out
		FString outLeftString, outRightString;
		actorName.Split("_", &outLeftString, &outRightString);
		if (!actorName.Contains(TargetCharacterPrefix) || !outRightString.IsNumeric())
		{
			UE_LOG(LogTemp, Warning, TEXT("Found camera actor named %s but ignoring it because it doesn't match requirements"), *actorName );
			continue;
		}

		CharacterCaptureInfo& newCharacter = m_allCharactersToCapture.AddDefaulted_GetRef();
		newCharacter.skinnedMeshComponent = characterActor->GetMesh();
		newCharacter.skinnedMeshComponent->GetBoneNames(newCharacter.boneNames);
		newCharacter.actorName = actorName;
		newCharacter.actorIndex = FCString::Atoi(*outRightString);
	}

	// Allocate the capture frame components
	// Preallocate all frames captured data
	m_framesCharactersCapture.Reserve(5000);
}

void AMotionLearnerActor::augmentAllCameraActors()
{
	TArray<AActor*> allActorsFound;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACameraActor::StaticClass(), allActorsFound);

	m_allCameraActors.Empty(allActorsFound.Num());

	for (AActor* actor : allActorsFound)
	{
		ACameraActor* cameraActor = Cast<ACameraActor, AActor>(actor);
		if (!cameraActor)
			continue;
		
		const FString actorName = cameraActor->GetName();

		//Filtering out
		FString outLeftString, outRightString;
		actorName.Split("_", &outLeftString, &outRightString);
		if (!actorName.Contains(TargetCameraPrefix) || !outRightString.IsNumeric())
		{
			UE_LOG(LogTemp, Warning, TEXT("Found camera actor named %s but ignoring it because it doesn't match requirements"), *actorName );
			continue;
		}
		
		const FString sceneCaptureCompStr = FString::Printf(TEXT("%s_%s"), *actorName, *TargetSceneCompSuffix);
		const FName sceneCaptureCompName(*sceneCaptureCompStr);
		
		// Add the augmented object
		CameraCaptureInfo info;
		prepareCaptureComponentForCamera(cameraActor, sceneCaptureCompName, info);
		info.actorName = actorName;
		info.cameraActor = cameraActor;		
		info.index = FCString::Atoi(*outRightString);
		
		m_allCameraActors.Add(info);
	}
}

void AMotionLearnerActor::captureFrameCharacters()
{
	// Push and preallocate a new frame
	AllCharactersFrameData& frameDataAll = m_framesCharactersCapture.AddDefaulted_GetRef();
	frameDataAll.frameid = m_frameCounter;
	frameDataAll.preallocate(m_allCharactersToCapture.Num());
	
	// For each registered character skeletal mesh, record its data
	for (CharacterCaptureInfo& charInfo : m_allCharactersToCapture)
	{
		if (charInfo.skinnedMeshComponent)
		{
			const int numBones = charInfo.skinnedMeshComponent->GetNumBones();
			check(charInfo.boneNames.Num() == numBones); // Hopefully didn't change

			CharacterCaptureInfo::FrameData& frameDataSingle = frameDataAll.capturedData.AddDefaulted_GetRef();
			frameDataSingle.actorIndex = charInfo.actorIndex;
			frameDataSingle.preallocate(numBones);
			
			for (int i = 0; i < numBones; i++)
			{
				CharacterCaptureInfo::BoneFrameData& boneFrameData = frameDataSingle.characterFrameData.AddDefaulted_GetRef();
				const FTransform& boneTransform = charInfo.skinnedMeshComponent->GetBoneTransform(i);
				boneFrameData.loc = boneTransform.GetLocation();
				boneFrameData.quat = boneTransform.GetRotation();
				boneFrameData.scale = boneTransform.GetScale3D();			
			}
		}
	}
}


void AMotionLearnerActor::prepareCaptureComponentForCamera(ACameraActor* cameraActor, const FName sceneCompName, CameraCaptureInfo& outCameraInfo)
{
	// Step 1: Create the scene capture component and attach it to the camera ACtor
	// component 0 is for RGB or RGB+Depth (depending on specs), component 1 is for Depth (depending on specs)
	// ----------------------------------------------------------------------------------------------------------
	USceneCaptureComponent2D* sceneCaptureComponent[2] = {nullptr, nullptr};
	sceneCaptureComponent[0] = NewObject<USceneCaptureComponent2D>(cameraActor, sceneCompName);
	sceneCaptureComponent[0]->AttachToComponent(cameraActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	sceneCaptureComponent[0]->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	check(sceneCaptureComponent[0]);	
	if (sceneCaptureComponent[0] == nullptr)
		return ;

	
#ifndef USE_COLORANDDEPTH
	FName sceneCompName_depth = FName(sceneCompName.ToString().Append("_depth"));
	sceneCaptureComponent[1] = NewObject<USceneCaptureComponent2D>(cameraActor, sceneCompName_depth);
	sceneCaptureComponent[1]->AttachToComponent(cameraActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	sceneCaptureComponent[1]->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	check(sceneCaptureComponent[1]);
	if (sceneCaptureComponent[1] == nullptr)
		return ;
#endif

	
	// register and setup both
	for (int i = 0; i < 2; i++)
	{
		if (sceneCaptureComponent[i] == nullptr)
			continue;
		
		sceneCaptureComponent[i]->RegisterComponent();
		//sceneCaptureComponent->SetupAttachment(cameraActor->GetRootComponent());
		sceneCaptureComponent[i]->bAutoActivate = true;
		sceneCaptureComponent[i]->bCaptureOnMovement = false;
		sceneCaptureComponent[i]->bCaptureEveryFrame = true;
	}

	// configure their capture sources
#ifdef USE_COLORANDDEPTH
	sceneCaptureComponent[0]->CaptureSource = SCS_SceneColorSceneDepth;
#else
	sceneCaptureComponent[0]->CaptureSource = SCS_FinalColorLDR; //SCS_SceneDepth;
	sceneCaptureComponent[1]->CaptureSource = SCS_SceneDepth; //SCS_DeviceDepth;
#endif

	// Step 1: Create the render target and attach it to the camera ACtor, set it as render target for the scene capture component
	// ----------------------------------------------------------------------------------------------------------
	FString renderTargetStr = FString::Printf(TEXT("%s_rndtarget"), *sceneCompName.ToString());
	FName renderTargetName(*renderTargetStr);
	UTextureRenderTarget2D* renderTarget[2] = {nullptr, nullptr};
	renderTarget[0] = NewObject<UTextureRenderTarget2D>(cameraActor, renderTargetName);

#ifndef USE_COLORANDDEPTH
	const FName renderTargetName_depth(*renderTargetStr.Append("_depth"));
	renderTarget[1] = NewObject<UTextureRenderTarget2D>(cameraActor, renderTargetName_depth);
#endif

	// 
#ifdef USE_COLORANDDEPTH
	renderTarget[0]->InitCustomFormat(ResolutionWidth, ResolutionHeight, EPixelFormat::PF_FloatRGBA, false);
	renderTarget[0]->RenderTargetFormat = RTF_RGBA16f;
#else
	renderTarget[0]->InitCustomFormat(ResolutionWidth, ResolutionHeight, EPixelFormat::PF_B8G8R8A8, false);
	renderTarget[0]->AddressX = TA_Wrap;
	renderTarget[0]->AddressY = TA_Wrap;
	renderTarget[0]->RenderTargetFormat = RTF_RGBA8;

	renderTarget[1]->InitAutoFormat(ResolutionWidth, ResolutionHeight);//InitCustomFormat(ResolutionWidth, ResolutionHeight, EPixelFormat::PF_R32_FLOAT, false);
	renderTarget[1]->AddressX = TA_Wrap;
	renderTarget[1]->AddressY = TA_Wrap;
	renderTarget[1]->RenderTargetFormat = RTF_R32f;	
#endif

	for (int i = 0; i < 2; i++)
	{
		if (renderTarget[i] == nullptr)
			continue;
		
		EPixelFormat pixelFormat = renderTarget[0]->GetFormat();
		pixelFormat = pixelFormat;
		
		renderTarget[i]->UpdateResourceImmediate(); // Should we call this manually ??

		sceneCaptureComponent[i]->TextureTarget = renderTarget[i];
	}

	// Write output
	for (int i = 0; i < 2; i++)
	{
		outCameraInfo.renderTarget2D[i] = renderTarget[i];
		outCameraInfo.sceneCaptureComponent2D[i] = sceneCaptureComponent[i];
	}
}

void AMotionLearnerActor::solveCamerasOutput()
{
	if (!EnabledSavingOnDisk)
		return;
	
	static bool saved[2] = {false};
	for (CameraCaptureInfo& cameraCapture : m_allCameraActors)
	{
		for (int sceneCamputerCompIndex = 0; sceneCamputerCompIndex < 2; sceneCamputerCompIndex++)
		{
			USceneCaptureComponent2D* sceneCapComp = cameraCapture.sceneCaptureComponent2D[sceneCamputerCompIndex];
			if (sceneCapComp == nullptr)
				continue;
			
			FTextureRenderTargetResource* renderTargetResource = sceneCapComp->TextureTarget->GameThread_GetRenderTargetResource();// m_renderTarget->GameThread_GetRenderTargetResource();
			if (renderTargetResource)
			{

				if (sceneCamputerCompIndex == 1)
				{

				#ifdef USE_COLORANDDEPTH
					renderTargetResource->ReadFloat16Pixels(m_SurfDataTemp);
				#else
					renderTargetResource->ReadLinearColorPixels(m_SurfDataTemp);
				#endif

					//renderTargetResource->Read

					//renderTargetResource->ReadPi
							
				#if 0 //example of postprocessinng- not used
					for (int y = 0; y < ResolutionHeight; y++)
						for (int x = 0; x < ResolutionWidth; x++)				
						{
							#ifdef USE_COLORANDDEPTH
							const FFloat16Color PixelColor = SurfData[x + y * ResolutionWidth];
							const FLinearColor linearColor(PixelColor);
							const float depth = linearColor.A;
							const FColor color = linearColor.ToFColor(false);
							if (depth < 65000)
							{
								int a = 3;
								a++;
							}
							#else
							static int debugT  = 500;
							
							FLinearColor color = SurfData[x + y * ResolutionWidth];
							if (color.R < debugT)
							{								
								color = color;
							}
							#endif
						}
				#endif
				}
				
				FString additionalSuffix = sceneCamputerCompIndex == 0 ? "_rgb" : "_depth";
				FString path_rgb = FString::Printf(TEXT("Cam_%02d_F%05d%s.png"), cameraCapture.index, m_frameCounter, *additionalSuffix);
				
				if (!cameraCapture.debug_savedOneFrame || EnabledSavingEachFrame)
				{
					if (sceneCamputerCompIndex == 0)
					{						
						UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), sceneCapComp->TextureTarget, FolderOutputPath, *path_rgb);
					}
					else
					{
						// Depth case - custom save
						FString TotalFileName = FPaths::Combine(FolderOutputPath, *path_rgb);
						FText PathError;
						FPaths::ValidatePath(TotalFileName, &PathError);
						FArchive* Ar = IFileManager::Get().CreateFileWriter(*TotalFileName);
					
						if (Ar)
						{
							// Convert from RGBA float to R only array to store efficiently...
							for (int y = 0; y < ResolutionHeight; y++)
								for (int x = 0; x < ResolutionWidth; x++)
								{
									const int index = x + y * ResolutionWidth;
									m_SurfDataDepth_Temp[index] = m_SurfDataTemp[index].R;
								}
							
							Ar->Serialize(m_SurfDataDepth_Temp.GetData(), m_SurfDataDepth_Temp.GetAllocatedSize());
							
							//FBufferArchive Buffer;
							//Ar.Serialize((void*)PNGData.GetData(), PNGData.GetAllocatedSize());
							//Ar->Serialize(const_cast<uint8*>(Buffer.GetData()), Buffer.Num());
		
							delete Ar;
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("Can't write a file to output path"), *TotalFileName);
						}
					}
				}
			}
		}

		cameraCapture.debug_savedOneFrame = true;
	}
}
