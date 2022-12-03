// Copyright (c) 2022 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once
#include "CoreMinimal.h"
#include "Components/SceneCaptureComponentCube.h"

#include "SceneCaptureComponentCube_CARLA.generated.h"



UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent, Mobility))
class CARLA_API USceneCaptureComponentCube_CARLA : public USceneCaptureComponentCube
{
	GENERATED_BODY()
public:

  USceneCaptureComponentCube_CARLA(const FObjectInitializer& = FObjectInitializer::Get());

  const AActor* ViewActor;

	virtual const AActor* GetViewOwner() const override;
};
