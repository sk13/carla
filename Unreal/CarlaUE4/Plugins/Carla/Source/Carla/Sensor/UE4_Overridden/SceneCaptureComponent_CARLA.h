// Copyright (c) 2022 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once
#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent.h"

#include "SceneCaptureComponent_CARLA.generated.h"



UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent, Mobility))
class CARLA_API USceneCaptureComponent_CARLA : public USceneCaptureComponent
{
	GENERATED_BODY()
public:

  USceneCaptureComponent_CARLA(const FObjectInitializer& = FObjectInitializer::Get());

  const AActor* ViewActor;

	virtual const AActor* GetViewOwner() const override;
};
