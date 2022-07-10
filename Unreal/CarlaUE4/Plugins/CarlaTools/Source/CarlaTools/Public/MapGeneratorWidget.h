// Copyright (c) 2022 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once 

#include "CoreMinimal.h"

#include "EditorUtilityWidget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ProceduralFoliageSpawner.h"
#include "UnrealString.h"

#include "MapGeneratorWidget.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCarlaToolsMapGenerator, Log, All);

/// Struct used as container of basic map information
USTRUCT(BlueprintType)
struct CARLATOOLS_API FMapGeneratorMetaInfo
{
  GENERATED_USTRUCT_BODY();

  UPROPERTY(BlueprintReadWrite)
  FString DestinationPath;

  UPROPERTY(BlueprintReadWrite)
  FString MapName;

  UPROPERTY(BlueprintReadWrite)   
  int SizeX;

  UPROPERTY(BlueprintReadWrite)
  int SizeY;

  UPROPERTY(BlueprintReadWrite)
  TArray<UProceduralFoliageSpawner*> FoliageSpawners;

  UPROPERTY(BlueprintReadWrite)
  UTextureRenderTarget2D* GlobalHeightmap;
};

/// Struct used as container of basic tile information
USTRUCT(BlueprintType)
struct CARLATOOLS_API FMapGeneratorTileMetaInfo
{
  GENERATED_USTRUCT_BODY();

  UPROPERTY(BlueprintReadWrite)
  FMapGeneratorMetaInfo MapMetaInfo;

  UPROPERTY(BlueprintReadWrite)   
  bool bIsTiled = true;

  UPROPERTY(BlueprintReadWrite)   
  int IndexX;

  UPROPERTY(BlueprintReadWrite)
  int IndexY;
};

/// Class UMapGeneratorWidget extends the functionality of UEditorUtilityWidget
/// to be able to generate and manage maps and largemaps tiles for procedural
/// map generation
UCLASS(BlueprintType)
class CARLATOOLS_API UMapGeneratorWidget : public UEditorUtilityWidget
{
  GENERATED_BODY()

public:
  /// Sets the specified material to @a Landscape
  UFUNCTION(BlueprintImplementableEvent)
  void AssignLandscapeMaterial(ALandscape* Landscape);

  /// PROVISIONAL
  UFUNCTION(BlueprintImplementableEvent)
  void UpdateTileRT(int OffsetX, int OffsetY);

  /// Function called by Widget Blueprint which generates all tiles of map
  /// @a mapName, and saves them in @a destinationPath
  /// Returns a void string is success and an error message if the process failed
  UFUNCTION(Category="Map Generator",BlueprintCallable)
  void GenerateMapFiles(const FMapGeneratorMetaInfo& MetaInfo);

  /// Function called by Widget Blueprint used to start the whole vegetation
  /// process for map defined in @a MetaInfo
  UFUNCTION(Category="Map Generator",BlueprintCallable)
  void CookVegetation(const FMapGeneratorMetaInfo& MetaInfo);

  /// Function invoked by the widget that cooks the vegetation defined in
  /// @a FoliageSpawners only in the world opened in the editor
  UFUNCTION(Category="Map Generator", BlueprintCallable)
  void CookVegetationToCurrentTile(const TArray<UProceduralFoliageSpawner*> FoliageSpawners);

  /// Recalculate the collisions of the loaded map
  UFUNCTION(Category="Map Generator", BlueprintCallable)
  bool RecalculateCollision();

  /// Traverse all tiles of map in @a MetaInfo and recalculates
  /// the collisions of all of them. Can be called from blueprints
  UFUNCTION(Category = "MapGenerator", BlueprintCallable)
  void CookTilesCollisions(const FMapGeneratorMetaInfo& MetaInfo);

  /// Utils funtion to format @a InDirectory so it gets sanitized in a 
  /// format that unreal can access the directory, deleting unnecesary 
  /// characters such as final '/' or '\'
  UFUNCTION(Category="Map Generator", BlueprintCallable)
  FString SanitizeDirectory(FString InDirectory);

  /// Used to find if the @a InDirectory contains a map. If does return true and
  /// @a OutMapSize and @OutFoundMapName stores the information of the found map 
  UFUNCTION(Category="Map Generator", BlueprintCallable)
  bool LoadMapInfoFromPath(FString InDirectory, int& OutMapSize, FString& OutFoundMapName);

  /// Spawns rivers of types @a RiverClass
  UFUNCTION(Category="MapGenerator", BlueprintCallable)
  AActor* GenerateWater(TSubclassOf<class AActor> RiverClass);

  /// Adds weather actor of type @a WeatherActorClass and sets the @a SelectedWeather
  /// to the map specified in @a MetaInfo
  UFUNCTION(Category="MapGenerator", BlueprintCallable)
  AActor* AddWeatherToExistingMap(TSubclassOf<class AActor> WeatherActorClass, 
      const FMapGeneratorMetaInfo& MetaInfo, const FString SelectedWeather);

private:  
  /// Loads a bunch of world objects located in @a BaseMapPath and 
  /// returns them in @a WorldAssetsData.
  /// The function returns true if success, otherwise false
  UFUNCTION()
  bool LoadWorlds(TArray<FAssetData>& WorldAssetsData, const FString& BaseMapPath, bool bRecursive = true);

  /// Saves @a WorldToBeSaved. It also saves all dirty packages to asure
  /// a correct managements of landscapes
  UFUNCTION()
  bool SaveWorld(UWorld* WorldToBeSaved);

  /// Takes the name of the map from @a MetaInfo and created the main map
  /// including all the actors needed by large map system
  UFUNCTION()
  bool CreateMainLargeMap(const FMapGeneratorMetaInfo& MetaInfo);

  /// Creates an Opendrive file for the created map map 
  /// specified in @a MetaInfo
  UFUNCTION()
  bool CreateOpenDriveFile(const FMapGeneratorMetaInfo& MetaInfo);

  /// Takes @a MetaInfo as input and generates all tiles based on the
  /// dimensions specified for the map
  /// The funtions return true is success, otherwise false
  UFUNCTION()
  bool CreateTilesMaps(const FMapGeneratorMetaInfo& MetaInfo);

  /// Searches for the specified map in the specified path in @a MetaInfo
  /// and starts the vegetation cooking process for each of the tile.
  /// IMPORTANT: Only maps with '_Tile_' tag in it name are processed as
  /// vegetation is only applied to tiles.
  /// The function returns true if success, otherwise false
  UFUNCTION()
  bool CookVegetationToTiles(const FMapGeneratorMetaInfo& MetaInfo);

  /// Instantiate a procedural foliage volume for each element in @a FoliageSpawners
  /// and cooks the corresponding vegetation to @a World
  /// Return true if success, false otherwise
  UFUNCTION()
  bool CookVegetationToWorld(UWorld* World, const TArray<UProceduralFoliageSpawner*> FoliageSpawners);

  /// Returns the world object in @a WorldAssetData
  UFUNCTION()
  UWorld* GetWorldFromAssetData(FAssetData& WorldAssetData);

  /// Returns the Z coordinate of the landscape located in @a World at position
  /// @a x and @a y.
  UFUNCTION()
  float GetLandscapeSurfaceHeight(UWorld* World, float x, float y, bool bDrawDebugLines);
};
