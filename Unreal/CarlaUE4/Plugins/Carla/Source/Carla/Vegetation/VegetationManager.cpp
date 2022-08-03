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
  return !BPFullClassName.IsEmpty() && SpawnedClass;
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
  BPClassName = "BP_" + Folder + FullVersion;
  BPFullClassName = "Blueprint'";
  for (int i = 0; i <= Position; ++i)
  {
    BPFullClassName += ParsedString[i];
    BPFullClassName += '/';
  }
  BPFullClassName += BPClassName;
  BPFullClassName += ".";
  BPFullClassName += BPClassName;
  BPFullClassName += "'";
  return true;
}

bool FFoliageBlueprint::SetSpawnedClass()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FoliageBlueprintCache::SetSpawnedClass);
  UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *BPFullClassName);
  UBlueprint* CastedBlueprint = Cast<UBlueprint>(LoadedObject);
  if (CastedBlueprint && CastedBlueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
  {
    SpawnedClass = *CastedBlueprint->GeneratedClass;  
    return true;
  }
  SpawnedClass = nullptr;
  return false;
}

/********************************************************************************/
/********** TILE DATA STRUCT ****************************************************/
/********************************************************************************/
void FTileData::UpdateMaterialCache(const FLinearColor& Value)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FTileData::UpdateMaterialCache);
  for (UMaterialInstanceDynamic* Material : MaterialInstanceDynamicCache)
  {
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
}

void AVegetationManager::Tick(float DeltaTime)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::Tick);
  {
    TRACE_CPUPROFILER_EVENT_SCOPE(Parent Tick);
    Super::Tick(DeltaTime);
  }  
  bool FoundVehicles = CheckIfAnyVehicleInLevel();
  if (!FoundVehicles)
    return;
  UpdateVehiclesDetectionBoxes();
  bool NewTilesFound = CheckForNewTiles();
  if (NewTilesFound)
  {
    UpdateFoliageBlueprintCache();
    UpdateTileDataCache();
    GenerateTileDataInternals();
  }
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
void AVegetationManager::UpdateTileDataCache()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::UpdateTileDataCache);
  const UObject* World = GetWorld();
  TArray<FString> NewKeys;
  TArray<AActor*> ActorsInLevel;
  UGameplayStatics::GetAllActorsOfClass(World, AInstancedFoliageActor::StaticClass(), ActorsInLevel);
  for (AActor* Actor : ActorsInLevel)
  {
    AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(Actor);
    if (!IsValid(InstancedFoliageActor))
      continue;
    const FString TileName = InstancedFoliageActor->GetLevel()->GetOuter()->GetName();
    if (TileDataCache.Contains(TileName))
    {
      UE_LOG(LogCarla, Warning, TEXT("Tile: %s is already in the cache."), *TileName);
      continue;
    }
    FTileData TileData {};
    TileData.InstancedFoliageActor = InstancedFoliageActor;
    TileDataCache.Emplace(TileName, TileData);
    NewKeys.Emplace(TileName);
  }

  ActorsInLevel.Reset();
  UGameplayStatics::GetAllActorsOfClass(World, AProceduralFoliageVolume::StaticClass(), ActorsInLevel);
  for (AActor* Actor : ActorsInLevel)
  {
    AProceduralFoliageVolume* Procedural = Cast<AProceduralFoliageVolume>(Actor);
    if (!IsValid(Procedural))
      continue;
    const FString TileName = Procedural->GetLevel()->GetOuter()->GetName();
    if (NewKeys.Contains(TileName))
    {
      FTileData* TileData = TileDataCache.Find(TileName);
      if (TileData)
      {
        TileData->ProceduralFoliageVolume = Procedural;
        UE_LOG(LogCarla, Display, TEXT("Tile: %s added to the cache."), *TileName);
      }
    }
  }
}

void AVegetationManager::UpdateFoliageBlueprintCache()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::UpdateFoliageBlueprintCache);
  const UObject* World = GetWorld();
  TArray<AActor*> ActorsInLevel;
  UGameplayStatics::GetAllActorsOfClass(World, AInstancedFoliageActor::StaticClass(), ActorsInLevel);
  for (AActor* Actor : ActorsInLevel)
  {
    AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(Actor);
    if (!IsValid(InstancedFoliageActor))
      continue;
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
      FoliageBlueprintCache.Emplace(Path, NewFoliageBlueprint);
      
      if (!NewFoliageBlueprint.IsValid())
      {     
        UE_LOG(LogCarla, Warning, TEXT("Blueprint %s was invalid."), *NewFoliageBlueprint.BPClassName);
      }
      else
      {
        UE_LOG(LogCarla, Display, TEXT("Blueprint %s created."), *NewFoliageBlueprint.BPClassName);
        CreatePoolForBPClass(NewFoliageBlueprint);
      }
    }
  }
}

void AVegetationManager::GenerateTileDataInternals()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::GenerateTileDataInternals);
  for (TPair<FString, FTileData>& Element : TileDataCache)
  {
    if (Element.Value.TileMeshesCache.Num() > 0)
      continue;
    InitializeInstancedStaticMeshComponentCache(Element.Value);
    InitializeMaterialCache(Element.Value);
  }
}

void AVegetationManager::InitializeInstancedStaticMeshComponentCache(FTileData& TileData)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::InitializeInstancedStaticMeshComponentCache);
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
    if (!BPCache->IsValid())
      continue;
    bool Found = false;
    for (auto& Element : TileData.TileMeshesCache)
    {
      if (Element.InstancedStaticMeshComponent == Mesh)
      {
        Found = true;
        break;
      }
    }
    if (Found)
      continue;
    FTileMeshComponent Aux;
    Aux.InstancedStaticMeshComponent = Mesh;
    TileData.TileMeshesCache.Emplace(Aux);
  }
}

void AVegetationManager::InitializeMaterialCache(FTileData& TileData)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::InitializeMaterialCache);
  const float Distance = VehiclesInLevel[0]->DetectionSize * 2.0f;
  for (FTileMeshComponent& Element : TileData.TileMeshesCache)
  {
    UInstancedStaticMeshComponent* Mesh = Element.InstancedStaticMeshComponent;
    UMaterialInterface* Material {nullptr};
    int32 Index {0};
    do
    {
      Material = Mesh->GetMaterial(Index);
      if (!Material)
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
      ++Index;
    } while (Material);
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
  FLinearColor Position = FLinearColor(VehiclesInLevel[0]->GetActorLocation());
  for (const FString& TileName : Tiles)
  {
    FTileData* TileData = TileDataCache.Find(TileName);
    if (!TileData)
      continue;
    TileData->UpdateMaterialCache(Position);
  }
}

TArray<TPair<FFoliageBlueprint, TArray<FTransform>>> AVegetationManager::GetElementsToSpawn(const TArray<FString>& Tiles)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::GetElementsToSpawn);
  TArray<TPair<FFoliageBlueprint, TArray<FTransform>>> Results;
  ALargeMapManager* LargeMap =
        UCarlaStatics::GetLargeMapManager(GetWorld());
      check(LargeMap);
  for (const FString& TileKey : Tiles)
  {
    FTileData* Tile = TileDataCache.Find(TileKey);
    if (!Tile)
      continue;
    for (FTileMeshComponent& Element : Tile->TileMeshesCache)
    {
      TRACE_CPUPROFILER_EVENT_SCOPE(Update Foliage Usage);
      UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Element.InstancedStaticMeshComponent;
      FString Path = InstancedStaticMeshComponent->GetStaticMesh()->GetPathName();
      FFoliageBlueprint* BP = FoliageBlueprintCache.Find(Path);
      if (!BP)
        continue;
      if (!BP->IsValid())
        continue;
      TArray<int32> Indices = VehiclesInLevel[0]->GetFoliageInstancesCloseToVehicle(InstancedStaticMeshComponent);
      if (Indices.Num() == 0)
        continue;
      for (int32 i = 0; i < Element.IndicesInUse.Num(); ++i)
      {
        if (Element.IndicesInUse[i] == -1)
          continue;
        if (Indices.Contains(Element.IndicesInUse[i]))
          continue;
        Element.IndicesInUse[i] = -1;
      }
      TPair<FFoliageBlueprint, TArray<FTransform>> NewElement {};
      NewElement.Key = *BP;
      TArray<FTransform> Transforms;
      for (int32 Index : Indices)
      {
        if (Element.IndicesInUse.Contains(Index))
          continue;
        FTransform Transform;
        InstancedStaticMeshComponent->GetInstanceTransform(Index, Transform, true);
        FTransform GlobalTransform = LargeMap->LocalToGlobalTransform(Transform);
        Transforms.Emplace(GlobalTransform);
        bool Found = false;
        //Add Index to Array
        for (int32 i = 0; i < Element.IndicesInUse.Num(); ++i)
        {
          if (Element.IndicesInUse[i] == -1)
          {
            Element.IndicesInUse[i] = Index;
            Found = true;
            break;
          }
        }
        if (!Found)
          Element.IndicesInUse.Emplace(Index);
        //----------------------------------------------------
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
  ALargeMapManager* LargeMap =
        UCarlaStatics::GetLargeMapManager(GetWorld());
      check(LargeMap);
  for (TPair<FFoliageBlueprint, TArray<FTransform>>& Element : ElementsToSpawn)
  {
    FFoliageBlueprint BP = Element.Key;
    if (!BP.IsValid())
      continue;
    TArray<FPooledActor>* Pool = ActorPool.Find(BP.BPClassName);
    if (!Pool)
      continue;
    for (const FTransform& Transform : Element.Value)
    {
      bool Ok = EnableActorFromPool(Transform, *Pool);
      if (Ok)
      {
        UE_LOG(LogCarla, Display, TEXT("Pooled actor from pool: %s"), *BP.BPClassName);
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
          UE_LOG(LogCarla, Display, TEXT("Created actor for pool: %s"), *BP.BPClassName);
        } 
      }
    }
  }
}

void AVegetationManager::DestroySkeletalFoliages()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::DestroySkeletalFoliages);
  ALargeMapManager* LargeMap =
        UCarlaStatics::GetLargeMapManager(GetWorld());
      check(LargeMap);
  for (TPair<FString, TArray<FPooledActor>>& Element : ActorPool)
  {
    TArray<FPooledActor>& Pool = Element.Value;
    for (FPooledActor& Actor : Pool)
    {
      if (!Actor.InUse)
          continue;
      const FVector Location = Actor.GlobalTransform.GetLocation();
      if (!VehiclesInLevel[0]->IsInVehicleRange(Location))
      {
        Actor.DisableActor();
        UE_LOG(LogCarla, Display, TEXT("Disabled Actor from pool"));
      }
    }
  }
}

bool AVegetationManager::EnableActorFromPool(const FTransform& Transform, TArray<FPooledActor>& Pool)
{
  ALargeMapManager* LargeMap =
        UCarlaStatics::GetLargeMapManager(GetWorld());
      check(LargeMap);
  for (FPooledActor& PooledActor : Pool)
  {
    if (PooledActor.InUse)
      continue;
    PooledActor.GlobalTransform = Transform;
    FTransform LocalTransform = LargeMap->GlobalToLocalTransform(Transform);
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
      UE_LOG(LogCarla, Display, TEXT("Created actor for pool: %s"), *BP.BPClassName);
      NewElement.DisableActor();
      AuxPool.Emplace(NewElement);
    }
    else
    {
      UE_LOG(LogCarla, Error, TEXT("Failed to create actor for pool: %s"), *BP.BPClassName);
    }
  }
  ActorPool.Emplace(BP.BPClassName, AuxPool);
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
  Actor->SetTickableWhenPaused(false);
  return Actor;
}

/********************************************************************************/
/********** OTHER ***************************************************************/
/********************************************************************************/
bool AVegetationManager::CheckIfAnyVehicleInLevel() const
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::CheckIfAnyVehicleInLevel);
  return VehiclesInLevel.Num() > 0;
}

bool AVegetationManager::IsFoliageTypeEnabled(const FString& Path) const
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::IsFoliageTypeEnabled);
  if (!SpawnRocks)
    if (Path.Contains("Rock"))
      return false;
  if (!SpawnTrees)
    if (Path.Contains("Tree"))
      return false;
  if (!SpawnBushes)
    if (Path.Contains("Bush"))
      return false;
  if (!SpawnPlants)
    if (Path.Contains("Plant"))
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
    if (!TileDataCache.Contains(TileName))
      return true;
  }
  return false;
}

TArray<FString> AVegetationManager::GetTilesInUse()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(AVegetationManager::GetTilesInUse);
  TArray<FString> Results;
  
  for (const TPair<FString, FTileData>& Element : TileDataCache)
  {
    const FTileData& TileData = Element.Value;
    if (!IsValid(TileData.InstancedFoliageActor) || !IsValid(TileData.ProceduralFoliageVolume))
    {
      TileDataCache.Remove(Element.Key);
      return Results;
    }
    if (Results.Contains(Element.Key))
      continue;
    const AProceduralFoliageVolume* Procedural = TileData.ProceduralFoliageVolume;
    if (!IsValid(Procedural))
      continue;    
    if (!IsValid(Procedural->ProceduralComponent))
      continue;
    if (Element.Key.IsEmpty())
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
