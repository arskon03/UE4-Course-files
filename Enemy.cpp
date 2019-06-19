// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy.h"
#include "Components/SphereComponent.h"
#include "AIController.h"
#include "MainCharacter.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Sound/SoundCue.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "MainPlayerController.h"


// Sets default values
AEnemy::AEnemy()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SetAgroSphere(CreateDefaultSubobject<USphereComponent>(TEXT("Enemy_AgroSphere")));
	GetAgroSphere()->SetupAttachment(GetRootComponent());
	GetAgroSphere()->InitSphereRadius(600.f);

	SetCombatSphere(CreateDefaultSubobject<USphereComponent>(TEXT("Enemy_CombatSphere")));
	GetCombatSphere()->SetupAttachment(GetRootComponent());
	GetCombatSphere()->InitSphereRadius(75.f);

	SetCombatCollision(CreateDefaultSubobject<UBoxComponent>(TEXT("Enemy_CombatCollision")));
	GetCombatCollision()->SetupAttachment(GetMesh(), FName("EnemySocket"));

	bOverlapingCombatSphere = false;

	Health = 75.f;
	MaxHealth = 100.f;
	Damage = 15.f;

	AttackMaxTime = 2.f;
	AttackMinTime = 0.5f;

	DeathDelay = 3.f;
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	GetCombatCollision()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCombatCollision()->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	GetCombatCollision()->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	GetCombatCollision()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	GetAgroSphere()->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::AgroSphereOnBeginOverlap);
	GetAgroSphere()->OnComponentEndOverlap.AddDynamic(this, &AEnemy::AgroSphereOnEndOverlap);

	GetCombatSphere()->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::CombatSphereOnBeginOverlap);
	GetCombatSphere()->OnComponentEndOverlap.AddDynamic(this, &AEnemy::CombatSphereOnEndOverlap);

	GetCombatCollision()->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::CombatOnBeginOverlap);
	GetCombatCollision()->OnComponentEndOverlap.AddDynamic(this, &AEnemy::CombatOnEndOverlap);
	
	SetAIController(Cast<AAIController>(GetController()));

	SetEnemyMovementStatus(EEnemyMovementStatus::EMS_Idle);
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const & DamageEvent, AController * EventInstigator, AActor * DamageCauser)
{
	if (Health - DamageAmount <= 0) 
	{
		Die();
	}
	Health -= DamageAmount;
	return DamageAmount;
}

void AEnemy::Die()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	SetEnemyMovementStatus(EEnemyMovementStatus::EMS_Dead);
	if (AnimInstance && CombatMontage)
	{
		AnimInstance->Montage_Play(CombatMontage, 1.0f);
		AnimInstance->Montage_JumpToSection(FName("Death"), CombatMontage);
	}
	
	CombatCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CombatSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AgroSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bAttacking = false;
}

bool AEnemy::Alive()
{
	return GetEnemyMovementStatus() != EEnemyMovementStatus::EMS_Dead;
}

void AEnemy::DeathEnd()
{
	UE_LOG(LogTemp, Warning, TEXT("DeathEnd"));
	GetMesh()->bPauseAnims = true;
	GetMesh()->bNoSkeletonUpdate = true;

	GetWorldTimerManager().SetTimer(DeathTimer, this, &AEnemy::Disappear, DeathDelay);
}

void AEnemy::AgroSphereOnBeginOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	if (OtherActor && Alive())
	{
		AMainCharacter* Character = Cast<AMainCharacter>(OtherActor);
		if (Character)
		{
			MoveToTarget(Character);
		}
	}
}

void AEnemy::AgroSphereOnEndOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor)
	{
		AMainCharacter* Character = Cast<AMainCharacter>(OtherActor);
		if (Character)
		{
			UE_LOG(LogTemp, Warning, TEXT("EMS_Idle"));
			Character->SetHasCombatTarget(false);
			SetEnemyMovementStatus(EEnemyMovementStatus::EMS_Idle);//
			if (AIController)
			{
				GetAIController()->StopMovement();
			}
		}
	}
}

void AEnemy::CombatSphereOnBeginOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	//UE_LOG(LogTemp, Warning, TEXT("1"));
	if (OtherActor && Alive())
	{
		//UE_LOG(LogTemp, Warning, TEXT("2"));
		AMainCharacter* Character = Cast<AMainCharacter>(OtherActor);
		if (Character)
		{
			//UE_LOG(LogTemp, Warning, TEXT("3"));
			Character->SetCombatTarget(this);
			Character->SetHasCombatTarget(true);
			if (Character->MainPlayerController)
			{
				Character->MainPlayerController->DisplayEnemyHealthBar();
			}

			CombatTarget = Character;
			bOverlapingCombatSphere = true;
			if (Character->Health > 0)
			{
				Attack();
			}
		}
	}
}

void AEnemy::CombatSphereOnEndOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex)
{
	//UE_LOG(LogTemp, Warning, TEXT("1"));
	if (OtherActor)
	{
		//UE_LOG(LogTemp, Warning, TEXT("2"));
		AMainCharacter* Character = Cast<AMainCharacter>(OtherActor);
		if (Character)
		{
			//UE_LOG(LogTemp, Warning, TEXT("3"));
			if (Character->CombatTarget == this)
			{
				Character->SetCombatTarget(nullptr);
			}
			bOverlapingCombatSphere = false;
			if (EnemyMovementStatus != EEnemyMovementStatus::EMS_Attacking)
			{
				MoveToTarget(Character);
				CombatTarget = nullptr;
			}
			if (Character->MainPlayerController)
			{
				Character->MainPlayerController->RemoveEnemyHealthBar();
			}
			SetEnemyMovementStatus(EEnemyMovementStatus::EMS_MoveToTarget);//
			MoveToTarget(Character);//
			GetWorldTimerManager().ClearTimer(AttackTimer);
		}
	}
}

void AEnemy::MoveToTarget(AMainCharacter * Target)
{
	SetEnemyMovementStatus(EEnemyMovementStatus::EMS_MoveToTarget);//
	
	if (AIController && Target->Health > 0)
	{
		FAIMoveRequest MoveRequest;
		MoveRequest.SetGoalActor(Target);
		MoveRequest.SetAcceptanceRadius(10.f);

		FNavPathSharedPtr NavPath;

		GetAIController()->MoveTo(MoveRequest, &NavPath);
		/*
		TArray<FNavPathPoint> PathPoints = NavPath->GetPathPoints();

		for (FNavPathPoint Point : PathPoints)
		{
			FVector Location = Point.Location;

			UKismetSystemLibrary::DrawDebugSphere(this, Location, 10.f, 8, FLinearColor::Red, 2.f, 1.f);
		}
		*/
	}
}

void AEnemy::ActivateCollision()
{
	UE_LOG(LogTemp, Warning, TEXT("ActivateCollision"));
	GetCombatCollision()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (GetSwingSound())
	{
		UGameplayStatics::PlaySound2D(this, SwingSound);
	}
}

void AEnemy::DeactivateCollision()
{
	UE_LOG(LogTemp, Warning, TEXT("DeactivateCollision"));
	GetCombatCollision()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemy::CombatOnBeginOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	UE_LOG(LogTemp, Warning, TEXT("CombatOnBeginOverlap1"));
	if (OtherActor)
	{
		AMainCharacter* Character = Cast<AMainCharacter>(OtherActor);
		UE_LOG(LogTemp, Warning, TEXT("CombatOnBeginOverlap2"));
		if (Character)
		{
			UE_LOG(LogTemp, Warning, TEXT("CombatOnBeginOverlap3"));
			if (Character->GetHitParticles())
			{
				UE_LOG(LogTemp, Warning, TEXT("CombatOnBeginOverlap4"));
				const USkeletalMeshSocket* TipSocket = GetMesh()->GetSocketByName("TipSocket");
				if (TipSocket)
				{
					UE_LOG(LogTemp, Warning, TEXT("CombatOnBeginOverlap5"));
					FVector SocketLocation = TipSocket->GetSocketLocation(GetMesh());
					UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Character->GetHitParticles(), SocketLocation, FRotator::ZeroRotator, false);
				}
			}
			if (Character->GetHitSound())
			{
				UE_LOG(LogTemp, Warning, TEXT("CombatOnBeginOverlap6"));
				UGameplayStatics::PlaySound2D(this, Character->GetHitSound());
			}
			if (DamageTypeClass)
			{
				UGameplayStatics::ApplyDamage(Character, Damage, AIController, this, DamageTypeClass);
			}
		}
	}
}

void AEnemy::CombatOnEndOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex)
{
}

void AEnemy::Attack()
{
	if (Alive())
	{
		if (AIController)
		{
			AIController->StopMovement();
			SetEnemyMovementStatus(EEnemyMovementStatus::EMS_Attacking);
		}
		if (CombatTarget)
		{
			if (!bAttacking && CombatTarget->Health > 0)
			{
				bAttacking = true;
				UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
				if (AnimInstance)
				{
					AnimInstance->Montage_Play(CombatMontage, 1.35f);
					AnimInstance->Montage_JumpToSection(FName("Attack"), CombatMontage);
				}
			}
		}
	}
}

void AEnemy::AttackEnd()
{
	bAttacking = false;
	if (bOverlapingCombatSphere)
	{
		float AttackTime = FMath::RandRange(AttackMinTime, AttackMaxTime);
		GetWorldTimerManager().SetTimer(AttackTimer, this, &AEnemy::Attack, AttackTime);
	}
}

void AEnemy::Disappear()
{
	Destroy();
}
