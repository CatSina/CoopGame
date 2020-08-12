// Fill out your copyright notice in the Description page of Project Settings.


#include "SWeapon.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "CoopGame/CoopGame.h"
#include "TimerManager.h"


static int32 DebugWeaponDrawing = 0;
FAutoConsoleVariableRef CVARDDebugWeaponDrawing(TEXT("COOP.DebugWeapons"),DebugWeaponDrawing,TEXT("Draw Debug Line for Weapon"),ECVF_Cheat);

// Sets default values
ASWeapon::ASWeapon()
{
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MuzzleSocketName = "MuzzleSocket";
	TracerTargetName = "Target";

	BaseDamage = 20.0f;
	RateOfFire = 600;

	TotalAmmo = 45;
	MaxMagAmmo = 20;
	CurrentAmmo = 20;

	UE_LOG(LogTemp, Warning, TEXT("TotalAmmo is %d"), TotalAmmo);

	bCanFire = true;

	
}

void ASWeapon::BeginPlay()
{
	Super::BeginPlay();

	TimeBetweenShots = 60 / RateOfFire;
}


void ASWeapon::Fire()
{
	CurrentAmmo -= 1;
	if (CurrentAmmo<=0)
	{
		bCanFire = false;
		StopFire();
	}

	UE_LOG(LogTemp, Warning, TEXT("current ammo %d"), CurrentAmmo);


	if (bCanFire)
		{
			AActor* MyOwner = GetOwner();
			if (MyOwner)
			{
				FVector EyeLocation;
				FRotator EyeRotation;
				MyOwner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

				FVector ShotDirection = EyeRotation.Vector();

				FVector TraceEnd = EyeLocation + (ShotDirection * 10000);

				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(MyOwner);
				QueryParams.AddIgnoredActor(this);
				QueryParams.bTraceComplex = true;
				QueryParams.bReturnPhysicalMaterial = true;

				//particle target param
				FVector TracerEndPoint = TraceEnd;

				FHitResult Hit;
				if (GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TraceEnd, COLLISION_WEAPOM, QueryParams))
				{

					AActor* HitActor = Hit.GetActor();


					EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

					float ActualDamage = BaseDamage;
					if (SurfaceType == SURFACE_FLESVULNERABLE)
					{
						ActualDamage *= 4.0f;
					}


					UGameplayStatics::ApplyPointDamage(HitActor, ActualDamage, ShotDirection, Hit, MyOwner->GetInstigatorController(), this, DamageType);



					UParticleSystem* SelectedEffect = nullptr;
					switch (SurfaceType)
					{
					case SURFACE_FLESHDEFAULT:
					case SURFACE_FLESVULNERABLE:
						SelectedEffect = FleshImpactEffect;
						break;
					default:
						SelectedEffect = DefaultImpactEffect;
						break;
					}

					if (SelectedEffect)
					{
						UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedEffect, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
					}

					TracerEndPoint = Hit.ImpactPoint;
				}
				if (DebugWeaponDrawing > 0)
				{
					DrawDebugLine(GetWorld(), EyeLocation, TraceEnd, FColor::White, false, 1.0f, 0, 1.0f);
				}
				PlayFireEffects(TracerEndPoint);

				LastFireTime = GetWorld()->TimeSeconds;


			}
		}
	
	

}

void ASWeapon::StartFire()
{
		float FirstDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);

		if (bCanFire)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_TimeBetweenShots, this, &ASWeapon::Fire, TimeBetweenShots, bCanFire, FirstDelay);
		}
}

void ASWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_TimeBetweenShots);
}

//do this 
void ASWeapon::WeaponReload()
{
	int32 ReloadingAmmo;
	ReloadingAmmo = MaxMagAmmo - CurrentAmmo;

	if (TotalAmmo>=ReloadingAmmo)
	{
		CurrentAmmo += ReloadingAmmo;
		TotalAmmo -= ReloadingAmmo;
		bCanFire = true;
	}
	else
	{
		CurrentAmmo += TotalAmmo;
		TotalAmmo = 0;
		bCanFire = false;
	}
			
	UE_LOG(LogTemp, Warning, TEXT("Total Ammo is : %d"),TotalAmmo);
	UE_LOG(LogTemp, Warning, TEXT("Reloading Ammo Is %d"),ReloadingAmmo);

}

void ASWeapon::PlayFireEffects(FVector TraceEnd)
{
	

	if (MuzzleEffect)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComp, MuzzleSocketName);
	}

	if (TracerEffect )
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);

		UParticleSystemComponent* TracerComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TracerEffect, MuzzleLocation);


		if (FireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), FireSound, MuzzleLocation, 1.0f, 1.0f, 0.0f);
		}


		if (TracerComp)
		{
			TracerComp->SetVectorParameter(TracerTargetName, TraceEnd);
		}
	}

	

	APawn* MyOwner = Cast<APawn>(GetOwner());
	if (MyOwner)
	{
		APlayerController* PC = Cast<APlayerController>(MyOwner->GetController());
		if (PC)
		{
			PC->ClientPlayCameraShake(FireCamShake);
		}
	}
}

