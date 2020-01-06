// This file is part of the FWorldGenenerator.
// Copyright Aleksandr "Flone" Tretyakov (github.com/Flone-dnb).
// Licensed under the ZLib license.
// Refer to the LICENSE file included.

#include "FWGen.h"

// STL
#include <random>
#include <ctime>

// External
#include "PerlinNoise.hpp"

#if WITH_EDITOR
#include "Components/BoxComponent.h"
#endif // WITH_EDITOR

#include "Components/StaticMeshComponent.h"



// --------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------



AFWGen::AFWGen()
{
	PrimaryActorTick.bCanEverTick = false;

	pChunkMap = nullptr;



	// Mesh structure

	pRootNode = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = pRootNode;

	//UProceduralMeshComponent* pCentralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	//pCentralMeshComponent->SetupAttachment(RootComponent);
	//pCentralMeshComponent->bUseAsyncCooking = true;




	// Water Plane

	WaterPlane = CreateDefaultSubobject<UStaticMeshComponent>("WaterPlane");
	WaterPlane->SetupAttachment(RootComponent);
	WaterPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("StaticMesh'/Engine/BasicShapes/Plane.Plane'"));
	WaterPlane->SetStaticMesh(PlaneMeshAsset.Object);

	if (CreateWater == false) WaterPlane->SetVisibility(false);




	// Materials

	WaterMaterial  = nullptr;
	GroundMaterial = nullptr;





	// Preview Plane

#if WITH_EDITOR
	PreviewPlane = CreateDefaultSubobject<UBoxComponent>("PreviewPlane");
	PreviewPlane->SetupAttachment(RootComponent);
	PreviewPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	refreshPreview();

	if (ComplexPreview) GenerateWorld();
#endif // WITH_EDITOR
}

AFWGen::~AFWGen()
{
	if (pChunkMap) delete pChunkMap;
}

bool AFWGen::GenerateWorld()
{
	if (pChunkMap == nullptr)
	{
		UProceduralMeshComponent* pMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
		pMeshComponent->bUseAsyncCooking = true;

		pChunkMap = new FWGenChunkMap(new FWGenChunk(EConnectionSide::CS_NONE, pMeshComponent), ViewDistance);
	}





	generateChunk(pChunkMap->getLastChunk());




	// Water Plane

	WaterPlane->SetWorldLocation(FVector(
		GetActorLocation().X,
		GetActorLocation().Y,
		GetActorLocation().Z + ((GenerationMaxZFromActorZ - GetActorLocation().Z) * ZWaterLevelInWorld)
	));

	WaterPlane->SetWorldScale3D(FVector(
		((ChunkPieceColumnCount) * ChunkPieceSizeX) * (WaterSize / 100.0f),
		((ChunkPieceRowCount) * ChunkPieceSizeY) * (WaterSize / 100.0f),
		0.1f
	));

	if (WaterMaterial)
	{
		WaterPlane->SetMaterial(0, WaterMaterial);
	}


	return false;
}

#if WITH_EDITOR
void AFWGen::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName MemberPropertyChanged = (PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None);

	if (
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ChunkPieceRowCount)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ChunkPieceColumnCount)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ChunkPieceSizeX)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ChunkPieceSizeY)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GenerationFrequency)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GenerationOctaves)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GenerationSeed)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GenerationMaxZFromActorZ)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ComplexPreview)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, InvertWorld)
		)
	{
		if (ChunkPieceRowCount < 1)
		{
			ChunkPieceRowCount = 1;
		}

		if (ChunkPieceColumnCount < 1)
		{
			ChunkPieceColumnCount = 1;
		}

		if (GenerationMaxZFromActorZ < GetActorLocation().Z)
		{
			GenerationMaxZFromActorZ = GetActorLocation().Z + 5.0f;
		}

		if (GenerationSeed < 0.0f)
		{
			GenerationSeed = 0.0f;
		}

		if (GenerationFrequency > 64.0f)
		{
			GenerationFrequency = 64.0f;
		}
		else if (GenerationFrequency < 0.1f)
		{
			GenerationFrequency = 0.1f;
		}

		if (GenerationOctaves > 16)
		{
			GenerationOctaves = 16;
		}
		else if (GenerationOctaves < 1)
		{
			GenerationOctaves = 1;
		}

		refreshPreview();
		if (ComplexPreview) GenerateWorld();
		else
		{
			if (pChunkMap)
			{
				FWGenChunk* pLastChunk = pChunkMap->getLastChunk();

				pLastChunk->vVertices.Empty();
				pLastChunk->vTriangles.Empty();

				pLastChunk->pMeshComponent->ClearAllMeshSections();
			}
		}
	}
	else if (
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, CreateWater)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ZWaterLevelInWorld)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, WaterSize)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, WaterMaterial)
		||
		MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, GroundMaterial)
		)
	{
		if (ZWaterLevelInWorld < 0.0f)
		{
			ZWaterLevelInWorld = 0.0f;
		}
		else if (ZWaterLevelInWorld > 1.0f)
		{
			ZWaterLevelInWorld = 1.0f;
		}

		if (CreateWater)
		{
			if (GroundMaterial && pChunkMap)
			{
				FWGenChunk* pLastChunk = pChunkMap->getLastChunk();

				pLastChunk->pMeshComponent->SetMaterial(0, GroundMaterial);
			}

			if (WaterMaterial)
			{
				WaterPlane->SetMaterial(0, WaterMaterial);
			}

			WaterPlane->SetVisibility(true);

			WaterPlane->SetWorldLocation(FVector(
				GetActorLocation().X,
				GetActorLocation().Y,
				GetActorLocation().Z + ((GenerationMaxZFromActorZ - GetActorLocation().Z) * ZWaterLevelInWorld)
			));
			WaterPlane->SetWorldScale3D(FVector(
				((ChunkPieceColumnCount) * ChunkPieceSizeX) * (WaterSize / 100.0f),
				((ChunkPieceRowCount) * ChunkPieceSizeY)    * (WaterSize / 100.0f),
				0.1f
			));
		}
		else
		{
			WaterPlane->SetVisibility(false);
		}
	}
	else if (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AFWGen, ViewDistance))
	{
		if (ViewDistance < 1)
		{
			ViewDistance = 1;
		}
		else
		{
			delete pChunkMap;
			pChunkMap = nullptr;

			if (ComplexPreview) GenerateWorld();
		}
	}
}
#endif // WITH_EDITOR

void AFWGen::BeginPlay()
{
	Super::BeginPlay();

}

void AFWGen::generateChunk(FWGenChunk* pChunk)
{
	// Generation setup

	uint32_t seed = 0;

	if ( GenerationSeed <= 0.0f )
	{
		// Generate seed
		std::mt19937_64 gen(std::random_device{}());
		std::uniform_int_distribution<unsigned int> uid(0, UINT_MAX);

		seed = uid(gen);
	}
	else
	{
		seed = GenerationSeed;
	}



	// We ++ here because we start to make polygons from 2nd row
	int32 iCorrectedRowCount = ChunkPieceRowCount + 1;
	int32 iCorrectedColumnCount = ChunkPieceColumnCount + 1;



	// Clear old data
	FWGenChunk* pLastChunk = pChunkMap->getLastChunk();
	pLastChunk->vVertices     .Empty();
	pLastChunk->vTriangles    .Empty();
	pLastChunk->vNormals      .Empty();
	pLastChunk->vUV0          .Empty();
	pLastChunk->vVertexColors .Empty();
	pLastChunk->vTangents     .Empty();




	// Perlin Noise setup

	const siv::PerlinNoise perlinNoise(seed);
	const double fx = ((iCorrectedColumnCount - 1) * ChunkPieceSizeX) / GenerationFrequency;
	const double fy = ((iCorrectedRowCount - 1) * ChunkPieceSizeY) / GenerationFrequency;




	// Generation params

	float fStartX = GetActorLocation().X - ((iCorrectedColumnCount - 1) * ChunkPieceSizeX) / 2;
	float fStartY = GetActorLocation().Y - ((iCorrectedRowCount    - 1) * ChunkPieceSizeY) / 2;

	FVector vPrevLocation(fStartX, fStartY, GetActorLocation().Z);

	float fInterval = GenerationMaxZFromActorZ - GetActorLocation().Z;




	// Generation

	for (int32 i = 0; i < iCorrectedRowCount; i++)
	{
		for (int32 j = 0; j < iCorrectedColumnCount; j++)
		{
			// Generate vertex
			double generatedValue = perlinNoise.octaveNoise0_1(vPrevLocation.X / fx, vPrevLocation.Y / fy, GenerationOctaves);

			if (InvertWorld)
			{
				// Here return value from perlinNoise.octaveNoise0_1 can be
				// 0    , but should be   1
				// 1    , but should be   0

				generatedValue = 1.0 - generatedValue;
			}

			// Here return value from perlinNoise.octaveNoise0_1 can be
			// 0    , but should be   GetActorLocation().Z
			// ...  , but should be   value from interval [GetActorLocation().Z; GenerationMaxZFromActorZ]
			// 1    , but should be   GenerationMaxZFromActorZ

			// So we do this:
			vPrevLocation.Z = GetActorLocation().Z + (fInterval * generatedValue);


			pLastChunk->vVertices .Add (vPrevLocation);

			pLastChunk->vNormals      .Add(FVector(0, 0, 1.0f));
			pLastChunk->vUV0          .Add(FVector2D(i, j));
			pLastChunk->vVertexColors .Add(FLinearColor(0.0f, 0.75, 0.0f, 1.0));
			pLastChunk->vTangents     .Add(FProcMeshTangent(0.0f, 1.0f, 0.0f));

			vPrevLocation.X += ChunkPieceSizeX;

			if (i != 0)
			{
				//     j = 0,   1,   2,   3  ...
				// i = 0:  +----+----+----+- ...
				//         |   /|   /|   /|
				//         |  / |  / |  / |
				//         | /  | /  | /  |
				// i = 1:  +----+----+----+- ...

				int32 iFirstIndexInRow = (i - 1) * iCorrectedColumnCount;

				if (j == 0)
				{
					// Add triangle #1
					pLastChunk->vTriangles.Add(iFirstIndexInRow + j);
					pLastChunk->vTriangles.Add(i * iCorrectedColumnCount + j);
					pLastChunk->vTriangles.Add(iFirstIndexInRow + j + 1);
				}
				else
				{
					// Add triangle #2
					pLastChunk->vTriangles.Add(iFirstIndexInRow + j);
					pLastChunk->vTriangles.Add(i * iCorrectedColumnCount + j - 1);
					pLastChunk->vTriangles.Add(i * iCorrectedColumnCount + j);

					if (j < (iCorrectedColumnCount - 1))
					{
						// Add triangle #1
						pLastChunk->vTriangles.Add(iFirstIndexInRow + j);
						pLastChunk->vTriangles.Add(i * iCorrectedColumnCount + j);
						pLastChunk->vTriangles.Add(iFirstIndexInRow + j + 1);
					}
				}
			}
		}

		vPrevLocation.Set(fStartX, vPrevLocation.Y + ChunkPieceSizeY, GetActorLocation().Z);
	}


	pLastChunk->pMeshComponent->ClearAllMeshSections();

	// pMeshComponent ->CreateMeshSection (0, vVertices, vTriangles, Bounds, true, EUpdateFrequency::Infrequent);
	pLastChunk->pMeshComponent->CreateMeshSection_LinearColor(0, pLastChunk->vVertices, pLastChunk->vTriangles, pLastChunk->vNormals,
		pLastChunk->vUV0, pLastChunk->vVertexColors, pLastChunk->vTangents, true);

	// Enable collision data
	pLastChunk->pMeshComponent->ContainsPhysicsTriMeshData(true);

	// Set material
	if (GroundMaterial)
	{
		pLastChunk->pMeshComponent->SetMaterial(0, GroundMaterial);
	}
}

#if WITH_EDITOR
void AFWGen::refreshPreview()
{
	PreviewPlane ->SetBoxExtent ( FVector (
		((ChunkPieceColumnCount) * ChunkPieceSizeX) / 2,
		((ChunkPieceRowCount) * ChunkPieceSizeY) / 2,
		GenerationMaxZFromActorZ / 2
	)
	);

	PreviewPlane ->SetRelativeLocation ( FVector (
		GetActorLocation () .X,
		GetActorLocation () .Y,
		GetActorLocation () .Z + (GenerationMaxZFromActorZ) / 2
	)   
	);
}
#endif // WITH_EDITOR
