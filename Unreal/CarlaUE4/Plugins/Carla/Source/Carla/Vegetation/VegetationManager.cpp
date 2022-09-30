// Copyright (c) 2022 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "ProceduralFoliageVolume.h"
#include "ProceduralFoliageComponent.h"

#include "Carla/Vegetation/VegetationManager.h"


static FString GetVersionFromFString(const FString& String)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(GetVersionFromFString);
  auto IsDigit = [](TCHAR charToCheck) {
      if (charToCheck == TCHAR('0')) return true;
      if (charToCheck == TCHAR('1')) return true;
      if (charToCheck == TCHAR('2')) return true;
      if (charToCheck == TCHAR('3')) return true;
      if (charToCheck == TCHAR('4')) return true;
      if (charToCheck == TCHAR('5')) return true;
      if (charToCheck == TCHAR('6')) return true;
      if (charToCheck == TCHAR('7')) return true;
      if (charToCheck == TCHAR('8')) return true;
      if (charToCheck == TCHAR('9')) return true;
      return false;
  };
  int index = String.Find(TEXT("_v"));
  if (index != -1)
  {
    index += 2;
    FString Version = "_v";
    while(IsDigit(String[index]))
    {
      Version += String[index];
      ++index;
      if (index == String.Len())
        return Version;
    }
    return Version;
  }
  return FString();
}

/********************************************************************************/
/********** POOLED ACTOR STRUCT *************************************************/
/********************************************************************************/
void FPooledActor::EnableActor()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FPooledActor::EnableActor);
  InUse = true;
  Actor->SetActorHiddenInGame(false);
  Actor->SetActorEnableCollision(true);
  Actor->SetActorTickEnabled(true);

  USpringBasedVegetationComponent* Component = Actor->FindComponentByClass<USpringBasedVegetationComponent>();
  if (Component)
  {
    Component->ResetComponent();
    Component->SetComponentTickEnabled(true);
  }
}

void FPooledActor::DisableActor()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FPooledActor::DisableActor);
  InUse = false;
  Actor->SetActorTransform(FTransform());
  Actor->SetActorHiddenInGame(true);
  Actor->SetActorEnableCollision(false);
  Actor->SetActorTickEnabled(false);

  USpringBasedVegetationComponent* Component = Actor->FindComponentByClass<USpringBasedVegetationComponent>();
  if (Component)
    Component->SetComponentTickEnabled(false);
}

/********************************************************************************/
/********** FOLIAGE BLUEPRINT STRUCT ********************************************/
/********************************************************************************/
bool FFoliageBlueprint::IsValid() const
{
  if (BPFullClassName.IsEmpty() || !BPFullClassName.Contains("_C"))
    return false;
  return SpawnedClass != nullptr;
}

bool FFoliageBlueprint::SetBPClassName(const FString& Path)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FoliageBlueprintCache::SetBPClassName);
  if (Path.IsEmpty())
    return false;
  TArray< FString > ParsedString;
  Path.ParseIntoArray(ParsedString, TEXT("/"), false);
  int Position = ParsedString.Num() - 1;
  const FString FullVersion = GetVersionFromFString(ParsedString[Position]);
  const FString Folder = ParsedString[--Position];
  const FString BPClassName = "BP_" + Folder + FullVersion;
  BPFullClassName = "Blueprint'";
  for (int i = 0; i <= Position; ++i)
  {
    BPFullClassName += ParsedString[i];
    BPFullClassName += '/';
  }
  BPFullClassName += BPClassName;
  BPFullClassName += ".";
  BPFullClassName += BPClassName;
  BPFullClassName += "_C'";
  return true;
}

bool FFoliageBlueprint::SetSpawnedClass()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FoliageBlueprintCache::SetSpawnedClass);
  UClass* CastedBlueprint = LoadObject< UClass >(nullptr, *BPFullClassName);
  if (CastedBlueprint)
  {
    SpawnedClass = CastedBlueprint;  
    return true;
  }
  SpawnedClass = nullptr;
  return false;
}

/********************************************************************************/
/********** TILE DATA STRUCT ****************************************************/
/********************************************************************************/
void FTileData::UpdateTileMeshComponent(UInstancedStaticMeshComponent* NewInstancedStaticMeshComponent)
{
  UInstancedStaticMeshComponent* Aux { nullptr };
  for (FTileMeshComponent& Element : TileMeshesCache)
  {
    if (Element.InstancedStaticMeshComponent == NewInstancedStaticMeshComponent)
    {
      int32 CurrentCount = Element.InstancedStaticMeshComponent->GetInstanceCount();
      int32 NewCount = NewInstancedStaticMeshComponent->GetInstanceCount();
      if (NewCount > CurrentCount)
      {
        Element.InstancedStaticMeshComponent = NewInstancedStaticMeshComponent;
        Element.IndicesInUse.Empty();
      }
    }
  }
}

bool FTileData::ContainsMesh(const UInstancedStaticMeshComponent* Mesh) const
{
  for (const FTileMeshComponent& Element : TileMeshesCache)
  {
    if (Element.InstancedStaticMeshComponent == Mesh)
      return true;
  }
  return false;
}

void FTileData::UpdateMaterialCache(const FLinearColor& Value, bool DebugMaterials)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FTileData::UpdateMaterialCache);
  for (UMaterialInstanceDynamic* Material : MaterialInstanceDynamicCache)
  {
    if (DebugMaterials)
      Material->SetScalarParameterValue("ActivateDebug", 1);
    else
      Material->SetScalarParameterValue("ActivateDebug", 0);
    Material->SetScalarParameterValue("ActivateOpacity", 1);
    Material->SetVectorParameterValue("VehiclePosition", Value);
  }
}

/********************************************************************************/
/********** OVERRIDE FROM ACTOR *************************************************/
/********************************************************************************/
void AVegetationManager::BeginPlay()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::BeginPlay);
  Super::BeginPlay();
  LargeMap = UCarlaStatics::GetLargeMapManager(GetWorld());
  FWorldDelegates::LevelAddedToWorld.AddUObject(this, &AVegetationManager::OnLevelAddedToWorld);
  FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &AVegetationManager::OnLevelRemovedFromWorld);
}

void AVegetationManager::Tick(float DeltaTime)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::Tick);
  {
    TRACE_CPUPROFILER_EVENT_SCOPE(Parent Tick);
    Super::Tick(DeltaTime);
  }
  if (!LargeMap)
    return;
  bool FoundVehicles = CheckIfAnyVehicleInLevel();
  if (!FoundVehicles)
    return;

  UpdateVehiclesDetectionBoxes();
  
  TArray<FString> TilesInUse = GetTilesInUse();
  if (TilesInUse.Num() == 0)
    return;
  
  UpdateMaterials(TilesInUse);
  TArray<TPair<FFoliageBlueprint, TArray<FTransform>>> ElementsToSpawn = GetElementsToSpawn(TilesInUse);
  SpawnSkeletalFoliages(ElementsToSpawn);
  DestroySkeletalFoliages();
}

/********************************************************************************/
/********** VEHICLE *************************************************************/
/********************************************************************************/
void AVegetationManager::AddVehicle(ACarlaWheeledVehicle* Vehicle)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::AddVehicle);
  if (!IsValid(Vehicle))
    return;
  if (VehiclesInLevel.Contains(Vehicle))
    return;
  VehiclesInLevel.Emplace(Vehicle);
  UE_LOG(LogCarla, Display, TEXT("Vehicle added."));
}

void AVegetationManager::RemoveVehicle(ACarlaWheeledVehicle* Vehicle)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::RemoveVehicle);
  if (!IsValid(Vehicle))
    return;
  if (!VehiclesInLevel.Contains(Vehicle))
    return;
  VehiclesInLevel.RemoveSingle(Vehicle);
  UE_LOG(LogCarla, Display, TEXT("Vehicle removed."));
}

/********************************************************************************/
/********** CACHES **************************************************************/
/********************************************************************************/
void AVegetationManager::CreateOrUpdateTileCache(ULevel* InLevel)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::CreateOrUpdateTileCache);
  FTileData TileData {};
  for (AActor* Actor : InLevel->Actors)
  {
    AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(Actor);
    if (!IsValid(InstancedFoliageActor))
      continue;
    TileData.InstancedFoliageActor = InstancedFoliageActor;
    break;
  }

  for (AActor* Actor : InLevel->Actors)
  {
    AProceduralFoliageVolume* ProceduralFoliageVolume = Cast<AProceduralFoliageVolume>(Actor);
    if (!IsValid(ProceduralFoliageVolume))
      continue;
    TileData.ProceduralFoliageVolume = ProceduralFoliageVolume;
    break;
  }
  const FString TileName = TileData.InstancedFoliageActor->GetLevel()->GetOuter()->GetName();
  FTileData* ExistingTileData = TileCache.Find(TileName);
  if (ExistingTileData)
  {
    ExistingTileData->InstancedFoliageActor = TileData.InstancedFoliageActor;
    ExistingTileData->ProceduralFoliageVolume = TileData.ProceduralFoliageVolume;
    SetTileDataInternals(*ExistingTileData);
  }
  else
  {
    SetTileDataInternals(TileData);
    TileCache.Emplace(TileName, TileData);
  }

}

void AVegetationManager::SetTileDataInternals(FTileData& TileData)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::SetTileDataInternals);
  SetInstancedStaticMeshComponentCache(TileData);
  SetMaterialCache(TileData);
}

void AVegetationManager::SetInstancedStaticMeshComponentCache(FTileData& TileData)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::SetInstancedStaticMeshComponentCache);
  const TSet<UActorComponent*>& ActorComponents = TileData.InstancedFoliageActor->GetComponents();
  for (UActorComponent* Component : ActorComponents)
  {
    UInstancedStaticMeshComponent* Mesh = Cast<UInstancedStaticMeshComponent>(Component);
    if (!IsValid(Mesh))
      continue;
    const FString Path = Mesh->GetStaticMesh()->GetPathName();
    const FFoliageBlueprint* BPCache = FoliageBlueprintCache.Find(Path);
    if (!BPCache)
      continue;

    if (TileData.ContainsMesh(Mesh))
    {
      TileData.UpdateTileMeshComponent(Mesh);
    }
    else
    {
      FTileMeshComponent Aux;
      Aux.InstancedStaticMeshComponent = Mesh;
      TileData.TileMeshesCache.Emplace(Aux);
    }
  }
}

void AVegetationManager::SetMaterialCache(FTileData& TileData)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::SetMaterialCache);
  if (TileData.MaterialInstanceDynamicCache.Num() > 0)
    TileData.MaterialInstanceDynamicCache.Empty();
  
  const float Distance = VehiclesInLevel.Last()->DetectionSize * 2.0f;
  for (FTileMeshComponent& Element : TileData.TileMeshesCache)
  {
    UInstancedStaticMeshComponent* Mesh = Element.InstancedStaticMeshComponent;
    int32 Index = -1;
    for (UMaterialInterface* Material : Mesh->GetMaterials())
    {
      ++Index;
      if (!IsValid(Material))
        continue;
      UMaterialInstanceDynamic* MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(Material, this);
      if (!MaterialInstanceDynamic)
        continue;
      if (TileData.MaterialInstanceDynamicCache.Contains(MaterialInstanceDynamic))
        continue;
      MaterialInstanceDynamic->SetScalarParameterValue("ActivateOpacity", 0);
      MaterialInstanceDynamic->SetScalarParameterValue("ActivateDebug", 0);
      MaterialInstanceDynamic->SetScalarParameterValue("Distance", Distance);
      Mesh->SetMaterial(Index, MaterialInstanceDynamic);
      TileData.MaterialInstanceDynamicCache.Emplace(MaterialInstanceDynamic);
    }
  }  
}

void AVegetationManager::UpdateFoliageBlueprintCache(ULevel* InLevel)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::UpdateFoliageBlueprintCache);
  for (AActor* Actor : InLevel->Actors)
  {
    AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(Actor);
    if (!IsValid(InstancedFoliageActor))
      continue;
    const FString TileName = InstancedFoliageActor->GetLevel()->GetOuter()->GetName();
    const TSet<UActorComponent*>& ActorComponents = InstancedFoliageActor->GetComponents();
    for (UActorComponent* Component : ActorComponents)
    {
      UInstancedStaticMeshComponent* Mesh = Cast<UInstancedStaticMeshComponent>(Component);
      if (!IsValid(Mesh))
        continue;      
      const FString Path = Mesh->GetStaticMesh()->GetPathName();
      if (!IsFoliageTypeEnabled(Path))
        continue;
      if (FoliageBlueprintCache.Contains(Path))
        continue;
      FFoliageBlueprint NewFoliageBlueprint;
      NewFoliageBlueprint.SetBPClassName(Path);
      NewFoliageBlueprint.SetSpawnedClass();
      
      if (!NewFoliageBlueprint.IsValid())
      {
        UE_LOG(LogCarla, Error, TEXT("Blueprint %s was invalid."), *NewFoliageBlueprint.BPFullClassName);
      }
      else
      {
        UE_LOG(LogCarla, Display, TEXT("Blueprint %s created."), *NewFoliageBlueprint.BPFullClassName);
        FoliageBlueprintCache.Emplace(Path, NewFoliageBlueprint);
        CreatePoolForBPClass(NewFoliageBlueprint);
      }
    }
  }
}

void AVegetationManager::FreeTileCache(ULevel* InLevel)
{
  FTileData TileData {};
  for (AActor* Actor : InLevel->Actors)
  {
    AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(Actor);
    if (!IsValid(InstancedFoliageActor))
      continue;
    TileData.InstancedFoliageActor = InstancedFoliageActor;
    break;
  }
  if (TileData.InstancedFoliageActor == nullptr)
    return;
  const FString TileName = TileData.InstancedFoliageActor->GetLevel()->GetOuter()->GetName();
  FTileData* ExistingTileData = TileCache.Find(TileName);
  if (ExistingTileData)
  {
    ExistingTileData->MaterialInstanceDynamicCache.Empty();
    for (FTileMeshComponent& Element : ExistingTileData->TileMeshesCache)
    {
      Element.IndicesInUse.Empty();
    }
    ExistingTileData->TileMeshesCache.Empty();
    TileCache.Remove(TileName);
  }
}

/********************************************************************************/
/********** TICK ****************************************************************/
/********************************************************************************/
void AVegetationManager::UpdateVehiclesDetectionBoxes()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::UpdateVehiclesDetectionBoxes);
  for (ACarlaWheeledVehicle* Vehicle : VehiclesInLevel)
    Vehicle->UpdateDetectionBox();
}

void AVegetationManager::UpdateMaterials(TArray<FString>& Tiles)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::UpdateMaterials);
  const FLinearColor Position = VehiclesInLevel.Last()->GetActorLocation();
  for (const FString& TileName : Tiles)
  {
    FTileData* TileData = TileCache.Find(TileName);
    if (!TileData)
      continue;
    TileData->UpdateMaterialCache(Position, DebugMaterials);
  }
}

TArray<TPair<FFoliageBlueprint, TArray<FTransform>>> AVegetationManager::GetElementsToSpawn(const TArray<FString>& Tiles)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::GetElementsToSpawn);
  TArray<TPair<FFoliageBlueprint, TArray<FTransform>>> Results;
  for (const FString& TileKey : Tiles)
  {
    FTileData* Tile = TileCache.Find(TileKey);
    if (!Tile)
      continue;
    for (FTileMeshComponent& Element : Tile->TileMeshesCache)
    {
      TRACE_CPUPROFILER_EVENT_SCOPE(Update Foliage Usage);
      UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Element.InstancedStaticMeshComponent;
      const FString Path = InstancedStaticMeshComponent->GetStaticMesh()->GetPathName();
      FFoliageBlueprint* BP = FoliageBlueprintCache.Find(Path);
      if (!BP)
        continue;
      TArray<int32> Indices = VehiclesInLevel.Last()->GetFoliageInstancesCloseToVehicle(InstancedStaticMeshComponent);
      if (Indices.Num() == 0)
        continue;

      TArray<int32> NewIndices;
      for (int32 Index : Indices)
      {
        if (Element.IndicesInUse.Contains(Index))
          continue;
        NewIndices.Emplace(Index);
      }
      Element.IndicesInUse = Indices;
      TPair<FFoliageBlueprint, TArray<FTransform>> NewElement {};
      NewElement.Key = *BP;
      TArray<FTransform> Transforms;
      for (int32 Index : NewIndices)
      {
        FTransform Transform;
        InstancedStaticMeshComponent->GetInstanceTransform(Index, Transform, true);
        FTransform GlobalTransform = LargeMap->LocalToGlobalTransform(Transform);
        Transforms.Emplace(GlobalTransform);
      }
      if (Transforms.Num() > 0)
      {
        NewElement.Value = Transforms;
        Results.Emplace(NewElement);
      }
    }
  }
  return Results;
}

void AVegetationManager::SpawnSkeletalFoliages(TArray<TPair<FFoliageBlueprint, TArray<FTransform>>>& ElementsToSpawn)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::SpawnSkeletalFoliages);
  for (TPair<FFoliageBlueprint, TArray<FTransform>>& Element : ElementsToSpawn)
  {
    FFoliageBlueprint BP = Element.Key;
    TArray<FPooledActor>* Pool = ActorPool.Find(BP.BPFullClassName);
    for (const FTransform& Transform : Element.Value)
    {
      bool Ok = EnableActorFromPool(Transform, *Pool);
      if (Ok)
      {
        UE_LOG(LogCarla, Display, TEXT("Pooled actor: %s"), *BP.BPFullClassName);
      }
      else
      {
        FPooledActor NewElement;
        NewElement.GlobalTransform = Transform;
        FTransform LocalTransform = LargeMap->GlobalToLocalTransform(Transform);
        NewElement.Actor = CreateFoliage(BP, LocalTransform);
        if (IsValid(NewElement.Actor))
        {
          NewElement.EnableActor();
          Pool->Emplace(NewElement);
          UE_LOG(LogCarla, Display, TEXT("Created actor: %s"), *BP.BPFullClassName);
        } 
      }
    }
  }
}

void AVegetationManager::DestroySkeletalFoliages()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::DestroySkeletalFoliages);
  for (TPair<FString, TArray<FPooledActor>>& Element : ActorPool)
  {
    TArray<FPooledActor>& Pool = Element.Value;
    for (FPooledActor& Actor : Pool)
    {
      if (!Actor.InUse)
          continue;
      const FVector Location = Actor.GlobalTransform.GetLocation();
      if (!VehiclesInLevel.Last()->IsInVehicleRange(Location))
      {
        Actor.DisableActor();
        UE_LOG(LogCarla, Display, TEXT("Disabled Actor"));
      }
    }
  }
}

bool AVegetationManager::EnableActorFromPool(const FTransform& Transform, TArray<FPooledActor>& Pool)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::EnableActorFromPool);
  for (FPooledActor& PooledActor : Pool)
  {
    if (PooledActor.InUse)
      continue;
    PooledActor.GlobalTransform = Transform;
    FTransform LocalTransform = Transform;
    LocalTransform = LargeMap->GlobalToLocalTransform(Transform);
    PooledActor.EnableActor();
    PooledActor.Actor->SetActorLocationAndRotation(LocalTransform.GetLocation(), LocalTransform.Rotator(), true, nullptr, ETeleportType::ResetPhysics);
    if (SpawnScale <= 1.01f && SpawnScale >= 0.99f)
      PooledActor.Actor->SetActorScale3D(LocalTransform.GetScale3D());
    else
      PooledActor.Actor->SetActorScale3D({SpawnScale, SpawnScale, SpawnScale});    
    return true;
  }
  return false;
}

/********************************************************************************/
/********** POOLS ***************************************************************/
/********************************************************************************/
void AVegetationManager::CreatePoolForBPClass(const FFoliageBlueprint& BP)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::CreatePoolForBPClass);
  TArray<FPooledActor> AuxPool;
  const FTransform Transform {};
  for (int32 i = 0; i < InitialPoolSize; ++i)
  {
    FPooledActor NewElement;
    NewElement.Actor = CreateFoliage(BP, Transform);
    if (IsValid(NewElement.Actor))
    {
      UE_LOG(LogCarla, Display, TEXT("Created actor for pool"));
      NewElement.DisableActor();
      AuxPool.Emplace(NewElement);
    }
    else
    {
      UE_LOG(LogCarla, Error, TEXT("Failed to create actor for pool"));
    }
  }
  ActorPool.Emplace(BP.BPFullClassName, AuxPool);
  UE_LOG(LogCarla, Display, TEXT("CreatePoolForBPClass: %s"), *BP.BPFullClassName);
}

AActor* AVegetationManager::CreateFoliage(const FFoliageBlueprint& BP, const FTransform& Transform) const
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::CreateFoliage);

  AActor* Actor = GetWorld()->SpawnActor<AActor>(BP.SpawnedClass,
    Transform.GetLocation(), Transform.Rotator());
  if (SpawnScale <= 1.01f && SpawnScale >= 0.99f)
    Actor->SetActorScale3D(Transform.GetScale3D());
  else
    Actor->SetActorScale3D({SpawnScale, SpawnScale, SpawnScale});
  return Actor;
}

/********************************************************************************/
/********** TILES ***************************************************************/
/********************************************************************************/
void AVegetationManager::OnLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::OnLevelAddedToWorld);
  UpdateFoliageBlueprintCache(InLevel);
  CreateOrUpdateTileCache(InLevel);
}

void AVegetationManager::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::OnLevelRemovedFromWorld);
  
}

bool AVegetationManager::CheckIfAnyVehicleInLevel() const
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::CheckIfAnyVehicleInLevel);
  return VehiclesInLevel.Num() > 0;
}

bool AVegetationManager::IsFoliageTypeEnabled(const FString& Path) const
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::IsFoliageTypeEnabled);
  if (!SpawnRocks)
    if (Path.Contains("/Rock/"))
      return false;
  if (!SpawnTrees)
    if (Path.Contains("/Tree/"))
      return false;
  if (!SpawnBushes)
    if (Path.Contains("/Bush/"))
      return false;
  if (!SpawnPlants)
    if (Path.Contains("/Plant/"))
      return false;
  return true;
}

bool AVegetationManager::CheckForNewTiles() const
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::CheckForNewTiles);
  const UObject* World = GetWorld();
  TArray<AActor*> ActorsInLevel;
  UGameplayStatics::GetAllActorsOfClass(World, AInstancedFoliageActor::StaticClass(), ActorsInLevel);
  for (AActor* Actor : ActorsInLevel)
  {
    AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(Actor);
    if (!IsValid(InstancedFoliageActor))
      continue;
    const FString TileName = InstancedFoliageActor->GetLevel()->GetOuter()->GetName();
    if (!TileCache.Contains(TileName))
      return true;
  }
  return false;
}

TArray<FString> AVegetationManager::GetTilesInUse()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::GetTilesInUse);
  TArray<FString> Results;
  
  for (const TPair<FString, FTileData>& Element : TileCache)
  {
    const FTileData& TileData = Element.Value;
    if (!IsValid(TileData.InstancedFoliageActor) || !IsValid(TileData.ProceduralFoliageVolume))
    {
      TileCache.Remove(Element.Key);
      return Results;
    }
    if (Results.Contains(Element.Key))
      continue;
    const AProceduralFoliageVolume* Procedural = TileData.ProceduralFoliageVolume;
    if (!IsValid(Procedural))
      continue;
    if (!IsValid(Procedural->ProceduralComponent))
      continue;
    const FBox Box = Procedural->ProceduralComponent->GetBounds();
    if (!Box.IsValid)
      continue;
    
    for (ACarlaWheeledVehicle* Vehicle : VehiclesInLevel)
    {
      if (!IsValid(Vehicle))
        continue;   
      if (Box.IsInside(Vehicle->GetActorLocation()))
      {
        Results.Emplace(Element.Key);
        break;
      }
    }
  }
  return Results;
}
