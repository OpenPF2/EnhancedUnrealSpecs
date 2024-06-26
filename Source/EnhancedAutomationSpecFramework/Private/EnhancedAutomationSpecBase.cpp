// Enhanced Automation Specs for UE, Copyright 2023-2024, Guy Elsmore-Paddock. All Rights Reserved.
//
// This Source Code Form is subject to the terms of the MIT License. If a copy of the license was not distributed with
// this file, you can obtain one at https://github.com/OpenPF2/EnhancedUnrealSpecs/blob/main/LICENSE.txt.
//
// Adapted from "Runtime/Core/Public/Misc/AutomationTest.h" (Unreal Engine 5.3), which is
// Copyright Epic Games, Inc. Licensed only for use with Unreal Engine.

#include "EnhancedAutomationSpecBase.h"

#include <IAutomationControllerModule.h>

// =====================================================================================================================
// FSimpleBlockingCommand
// =====================================================================================================================
bool FEnhancedAutomationSpecBase::FSimpleBlockingCommand::Update()
{
	if (!this->bSkipIfErrored || !this->Spec->HasAnyErrors())
	{
		this->Work();
	}

	return true;
}

// =====================================================================================================================
// FAsyncCommand
// =====================================================================================================================
bool FEnhancedAutomationSpecBase::FAsyncCommand::Update()
{
	if (!this->Future.IsValid())
	{
		if (this->bSkipIfErrored && this->Spec->HasAnyErrors())
		{
			return true;
		}

		this->bDone = false;

		this->Future = Async(this->Execution, [this]() {
			this->Work();
			this->Done();
		});

		this->StartTime = FDateTime::UtcNow();
	}

	if (this->bDone)
	{
		this->Reset();

		return true;
	}
	else if (FDateTime::UtcNow() >= (StartTime + Timeout))
	{
		this->Reset();
		this->Spec->AddError(TEXT("Latent command timed out."), 0);

		return true;
	}

	return false;
}

void FEnhancedAutomationSpecBase::FAsyncCommand::Done()
{
	if (this->Future.IsValid())
	{
		this->bDone = true;
	}
}

void FEnhancedAutomationSpecBase::FAsyncCommand::Reset()
{
	// Reset the status for the next potential run of this command.
	this->bDone = false;

	this->Future.Reset();
}

// =====================================================================================================================
// FMultiFrameLatentCommand
// =====================================================================================================================
bool FEnhancedAutomationSpecBase::FMultiFrameLatentCommand::Update()
{
	if (!this->bHasStartedRunning)
	{
		if (this->bSkipIfErrored && this->Spec->HasAnyErrors())
		{
			return true;
		}

		this->bDone = false;

		this->Work(FDoneDelegate::CreateSP(this, &FMultiFrameLatentCommand::Done));

		this->bHasStartedRunning = true;
		this->StartTime          = FDateTime::UtcNow();
	}

	if (this->bDone)
	{
		this->Reset();

		return true;
	}
	else if (FDateTime::UtcNow() >= (this->StartTime + this->Timeout))
	{
		this->Reset();
		this->Spec->AddError(TEXT("Latent command timed out."), 0);

		return true;
	}

	return false;
}

void FEnhancedAutomationSpecBase::FMultiFrameLatentCommand::Done()
{
	if (this->bHasStartedRunning)
	{
		this->bDone = true;
	}
}

void FEnhancedAutomationSpecBase::FMultiFrameLatentCommand::Reset()
{
	// Reset the status for the next potential run of this command.
	this->bDone              = false;
	this->bHasStartedRunning = false;
}

// =====================================================================================================================
// FAsyncMultiFrameLatentCommand
// =====================================================================================================================
bool FEnhancedAutomationSpecBase::FAsyncMultiFrameLatentCommand::Update()
{
	if (!this->Future.IsValid())
	{
		if (this->bSkipIfErrored && this->Spec->HasAnyErrors())
		{
			return true;
		}

		this->bDone = false;

		this->Future = Async(this->Execution, [this]() {
			this->Work(FDoneDelegate::CreateRaw(this, &FAsyncMultiFrameLatentCommand::Done));
		});

		this->StartTime = FDateTime::UtcNow();
	}

	if (this->bDone)
	{
		this->Reset();

		return true;
	}
	else if (FDateTime::UtcNow() >= (this->StartTime + this->Timeout))
	{
		this->Reset();
		this->Spec->AddError(TEXT("Latent command timed out."), 0);

		return true;
	}

	return false;
}

void FEnhancedAutomationSpecBase::FAsyncMultiFrameLatentCommand::Done()
{
	if (this->Future.IsValid())
	{
		this->bDone = true;
	}
}

void FEnhancedAutomationSpecBase::FAsyncMultiFrameLatentCommand::Reset()
{
	// Reset the status for the next potential run of this command.
	this->bDone = false;

	this->Future.Reset();
}

// =====================================================================================================================
// FSpecBlockHandle
// =====================================================================================================================
void FEnhancedAutomationSpecBase::FSpecBlockHandle::Assign()
{
	static int32 HandleCounter = 1;

	this->Handle = HandleCounter++;
}

// =====================================================================================================================
// FEnhancedTestSessionState
// =====================================================================================================================
FEnhancedAutomationSpecBase::FEnhancedTestSessionState::FEnhancedTestSessionState()
{
	const IAutomationControllerManagerPtr AutomationControllerManager = this->GetAutomationController();

	if (AutomationControllerManager.IsValid())
	{
		this->TestDelegateHandle =
			AutomationControllerManager->OnTestsComplete().AddRaw(this, &FEnhancedTestSessionState::ClearState);
	}
}

FEnhancedAutomationSpecBase::FEnhancedTestSessionState::~FEnhancedTestSessionState()
{
	const IAutomationControllerManagerPtr AutomationControllerManager = this->GetAutomationController();

	if (this->TestDelegateHandle.IsValid() && AutomationControllerManager.IsValid())
	{
		AutomationControllerManager->OnTestsComplete().Remove(this->TestDelegateHandle);
	}
}

bool FEnhancedAutomationSpecBase::FEnhancedTestSessionState::HasBlockRun(const FSpecBlockHandle& BlockHandle) const
{
	return this->BlocksRun.Contains(BlockHandle);
}

void FEnhancedAutomationSpecBase::FEnhancedTestSessionState::MarkBlockAsRun(const FSpecBlockHandle& BlockHandle)
{
	check(!this->HasBlockRun(BlockHandle));
	this->BlocksRun.Add(BlockHandle);
}

IAutomationControllerManagerPtr FEnhancedAutomationSpecBase::FEnhancedTestSessionState::GetAutomationController()
{
	IAutomationControllerManagerPtr Result;

	IAutomationControllerModule* AutomationControllerModule =
		FModuleManager::GetModulePtr<IAutomationControllerModule>(TEXT("AutomationController"));

	if (AutomationControllerModule != nullptr)
	{
		Result = AutomationControllerModule->GetAutomationController();
	}

	return Result;
}

void FEnhancedAutomationSpecBase::FEnhancedTestSessionState::ClearState()
{
	this->BlocksRun.Empty();
}

// =====================================================================================================================
// FEnhancedAutomationSpecBase
// =====================================================================================================================
FString FEnhancedAutomationSpecBase::GetTestSourceFileName(const FString& InTestName) const
{
	FString TestId = InTestName;

	if (TestId.StartsWith(this->TestName + TEXT(" ")))
	{
		TestId = InTestName.RightChop(this->TestName.Len() + 1);
	}

	const TSharedRef<FSpec>* Spec = this->IdToSpecMap.Find(TestId);

	if (Spec != nullptr)
	{
		return (*Spec)->Filename;
	}

	return FAutomationTestBase::GetTestSourceFileName();
}

int32 FEnhancedAutomationSpecBase::GetTestSourceFileLine(const FString& InTestName) const
{
	FString TestId = InTestName;

	if (TestId.StartsWith(this->TestName + TEXT(" ")))
	{
		TestId = InTestName.RightChop(this->TestName.Len() + 1);
	}

	const TSharedRef<FSpec>* Spec = this->IdToSpecMap.Find(TestId);

	if (Spec != nullptr)
	{
		return (*Spec)->LineNumber;
	}

	return FAutomationTestBase::GetTestSourceFileLine();
}

void FEnhancedAutomationSpecBase::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<TSharedRef<FSpec>> Specs;

	this->EnsureDefinitions();

	this->IdToSpecMap.GenerateValueArray(Specs);

	for (auto TestIterator = Specs.CreateIterator(); TestIterator; ++TestIterator)
	{
		const TSharedRef<FSpec> SpecToRun = *TestIterator;

		OutTestCommands.Push(SpecToRun->Id);
		OutBeautifiedNames.Push(SpecToRun->Description);
	}
}

bool FEnhancedAutomationSpecBase::RunTest(const FString& InParameters)
{
	this->EnsureDefinitions();

	if (!this->SuiteSessionState.IsValid())
	{
		this->SuiteSessionState = MakeShareable(new FEnhancedTestSessionState());
	}

	if (InParameters.IsEmpty())
	{
		TArray<TSharedRef<FSpec>> Specs;

		this->IdToSpecMap.GenerateValueArray(Specs);

		// Run all tests.
		for (auto TestIterator = Specs.CreateIterator(); TestIterator; ++TestIterator)
		{
			const TSharedRef<FSpec> SpecToRun = *TestIterator;

			this->RunSpec(SpecToRun);
		}
	}
	else
	{
		const TSharedRef<FSpec>* SpecToRun = this->IdToSpecMap.Find(InParameters);

		// Run specific test.
		if (SpecToRun != nullptr)
		{
			this->RunSpec(*SpecToRun);
		}
	}

	return true;
}

void FEnhancedAutomationSpecBase::Describe(const FString& InDescription, const TFunction<void()>& DoWork)
{
	LLM_SCOPE_BYNAME(TEXT("AutomationTest/Framework"));

	const TSharedRef<FSpecDefinitionScope> ParentScope = this->DefinitionScopeStack.Last();
	const TSharedRef<FSpecDefinitionScope> NewScope    = MakeShareable(new FSpecDefinitionScope());

	NewScope->Description = InDescription;

	ParentScope->Children.Push(NewScope);

	this->DefinitionScopeStack.Push(NewScope);
	this->PushDescription(InDescription);

	DoWork();

	this->PopDescription();
	this->DefinitionScopeStack.Pop();

	if (NewScope->It.Num() == 0 && NewScope->Children.Num() == 0)
	{
		ParentScope->Children.Remove(NewScope);
	}
}

void FEnhancedAutomationSpecBase::It(const FString& InDescription, const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();
	const FProgramCounterSymbolInfo&       TopOfStack   = GetTopOfCallStack();

	this->PushDescription(InDescription);

	CurrentScope->It.Push(
		MakeShareable(
			new FSpecItDefinition(
				this->GetId(),
				this->GetDescription(),
				TopOfStack.Filename,
				TopOfStack.LineNumber,
				MakeShareable(new FSimpleBlockingCommand(this, DoWork, this->bEnableSkipIfError))
			)
		)
	);

	this->PopDescription();
}

void FEnhancedAutomationSpecBase::It(const FString&           InDescription,
                                     const EAsyncExecution    Execution,
                                     const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();
	const FProgramCounterSymbolInfo&       TopOfStack   = GetTopOfCallStack();

	this->PushDescription(InDescription);

	CurrentScope->It.Push(
		MakeShareable(
			new FSpecItDefinition(
				this->GetId(),
				this->GetDescription(),
				TopOfStack.Filename,
				TopOfStack.LineNumber,
				MakeShareable(
					new FAsyncCommand(this, Execution, DoWork, this->DefaultTimeout, this->bEnableSkipIfError)
				)
			)
		)
	);

	this->PopDescription();
}

void FEnhancedAutomationSpecBase::It(const FString&           InDescription,
                                     const EAsyncExecution    Execution,
                                     const FTimespan&         Timeout,
                                     const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();
	const FProgramCounterSymbolInfo&       TopOfStack   = GetTopOfCallStack();

	this->PushDescription(InDescription);

	CurrentScope->It.Push(
		MakeShareable(
			new FSpecItDefinition(
				this->GetId(),
				this->GetDescription(),
				TopOfStack.Filename,
				TopOfStack.LineNumber,
				MakeShareable(new FAsyncCommand(this, Execution, DoWork, Timeout, this->bEnableSkipIfError))
			)
		)
	);

	this->PopDescription();
}

void FEnhancedAutomationSpecBase::LatentIt(const FString& InDescription,
                                           const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();
	const FProgramCounterSymbolInfo&       TopOfStack   = GetTopOfCallStack();

	this->PushDescription(InDescription);

	CurrentScope->It.Push(
		MakeShareable(
			new FSpecItDefinition(
				this->GetId(),
				this->GetDescription(),
				TopOfStack.Filename,
				TopOfStack.LineNumber,
				MakeShareable(
					new FMultiFrameLatentCommand(this, DoWork, this->DefaultTimeout, this->bEnableSkipIfError)
				)
			)
		)
	);

	this->PopDescription();
}

void FEnhancedAutomationSpecBase::LatentIt(const FString&                               InDescription,
                                           const FTimespan&                             Timeout,
                                           const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();
	const FProgramCounterSymbolInfo&       TopOfStack   = GetTopOfCallStack();

	this->PushDescription(InDescription);

	CurrentScope->It.Push(
		MakeShareable(
			new FSpecItDefinition(
				this->GetId(),
				this->GetDescription(),
				TopOfStack.Filename,
				TopOfStack.LineNumber,
				MakeShareable(new FMultiFrameLatentCommand(this, DoWork, Timeout, this->bEnableSkipIfError))
			)
		)
	);

	this->PopDescription();
}

void FEnhancedAutomationSpecBase::LatentIt(const FString&                               InDescription,
                                           const EAsyncExecution                        Execution,
                                           const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();
	const FProgramCounterSymbolInfo&       TopOfStack   = GetTopOfCallStack();

	this->PushDescription(InDescription);

	CurrentScope->It.Push(
		MakeShareable(
			new FSpecItDefinition(
				this->GetId(),
				this->GetDescription(),
				TopOfStack.Filename,
				TopOfStack.LineNumber,
				MakeShareable(
					new FAsyncMultiFrameLatentCommand(
						this,
						Execution,
						DoWork,
						this->DefaultTimeout,
						this->bEnableSkipIfError
					)
				)
			)
		)
	);

	this->PopDescription();
}

void FEnhancedAutomationSpecBase::LatentIt(const FString&                               InDescription,
                                           const EAsyncExecution                        Execution,
                                           const FTimespan&                             Timeout,
                                           const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();
	const FProgramCounterSymbolInfo&       TopOfStack   = GetTopOfCallStack();

	this->PushDescription(InDescription);

	CurrentScope->It.Push(
		MakeShareable(
			new FSpecItDefinition(
				this->GetId(),
				this->GetDescription(),
				TopOfStack.Filename,
				TopOfStack.LineNumber,
				MakeShareable(
					new FAsyncMultiFrameLatentCommand(this, Execution, DoWork, Timeout, this->bEnableSkipIfError)
				)
			)
		)
	);

	this->PopDescription();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void FEnhancedAutomationSpecBase::BeforeAll(const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->GetCurrentScope();
	const FSpecBlockHandle                 BlockHandle;
	const TFunction<void()>&               RunWorkOnce = this->CreateRunWorkOnceWrapper(BlockHandle, DoWork);

	CurrentScope->BeforeAll.Push(
		MakeShareable(new FSimpleBlockingCommand(this, RunWorkOnce, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::BeforeAll(const EAsyncExecution Execution, const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->GetCurrentScope();
	const FSpecBlockHandle                 BlockHandle;
	const TFunction<void()>&               RunWorkOnce = this->CreateRunWorkOnceWrapper(BlockHandle, DoWork);

	CurrentScope->BeforeAll.Push(
		MakeShareable(
			new FAsyncCommand(this, Execution, RunWorkOnce, this->DefaultTimeout, this->bEnableSkipIfError)
		)
	);
}

void FEnhancedAutomationSpecBase::BeforeAll(const EAsyncExecution    Execution,
                                            const FTimespan&         Timeout,
                                            const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->GetCurrentScope();
	const FSpecBlockHandle                 BlockHandle;
	const TFunction<void()>&               RunWorkOnce = this->CreateRunWorkOnceWrapper(BlockHandle, DoWork);

	CurrentScope->BeforeAll.Push(
		MakeShareable(new FAsyncCommand(this, Execution, RunWorkOnce, Timeout, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::LatentBeforeAll(const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope>       CurrentScope = this->GetCurrentScope();
	const FSpecBlockHandle                       BlockHandle;
	const TFunction<void(const FDoneDelegate&)>& RunWorkOnce = this->CreateRunWorkOnceWrapper(BlockHandle, DoWork);

	CurrentScope->BeforeAll.Push(
		MakeShareable(new FMultiFrameLatentCommand(this, RunWorkOnce, this->DefaultTimeout, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::LatentBeforeAll(const FTimespan&                             Timeout,
                                                  const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope>       CurrentScope = this->GetCurrentScope();
	const FSpecBlockHandle                       BlockHandle;
	const TFunction<void(const FDoneDelegate&)>& RunWorkOnce = this->CreateRunWorkOnceWrapper(BlockHandle, DoWork);

	CurrentScope->BeforeAll.Push(
		MakeShareable(new FMultiFrameLatentCommand(this, RunWorkOnce, Timeout, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::LatentBeforeAll(const EAsyncExecution                        Execution,
                                                  const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope>       CurrentScope = this->GetCurrentScope();
	const FSpecBlockHandle                       BlockHandle;
	const TFunction<void(const FDoneDelegate&)>& RunWorkOnce = this->CreateRunWorkOnceWrapper(BlockHandle, DoWork);

	CurrentScope->BeforeAll.Push(
		MakeShareable(
			new FAsyncMultiFrameLatentCommand(
				this,
				Execution,
				RunWorkOnce,
				this->DefaultTimeout,
				this->bEnableSkipIfError
			)
		)
	);
}

void FEnhancedAutomationSpecBase::LatentBeforeAll(const EAsyncExecution                        Execution,
                                                  const FTimespan&                             Timeout,
                                                  const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope>       CurrentScope = this->GetCurrentScope();
	const FSpecBlockHandle                       BlockHandle;
	const TFunction<void(const FDoneDelegate&)>& RunWorkOnce = this->CreateRunWorkOnceWrapper(BlockHandle, DoWork);

	CurrentScope->BeforeAll.Push(
		MakeShareable(
			new FAsyncMultiFrameLatentCommand(this, Execution, RunWorkOnce, Timeout, this->bEnableSkipIfError)
		)
	);
}

void FEnhancedAutomationSpecBase::BeforeEach(const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->BeforeEach.Push(
		MakeShareable(new FSimpleBlockingCommand(this, DoWork, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::BeforeEach(const EAsyncExecution Execution, const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->BeforeEach.Push(
		MakeShareable(new FAsyncCommand(this, Execution, DoWork, this->DefaultTimeout, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::BeforeEach(const EAsyncExecution Execution, const FTimespan& Timeout,
                                             const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->BeforeEach.Push(
		MakeShareable(new FAsyncCommand(this, Execution, DoWork, Timeout, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::LatentBeforeEach(const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->BeforeEach.Push(
		MakeShareable(new FMultiFrameLatentCommand(this, DoWork, this->DefaultTimeout, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::LatentBeforeEach(const FTimespan& Timeout,
                                                   const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->BeforeEach.Push(
		MakeShareable(new FMultiFrameLatentCommand(this, DoWork, Timeout, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::LatentBeforeEach(const EAsyncExecution Execution,
                                                   const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->BeforeEach.Push(
		MakeShareable(
			new FAsyncMultiFrameLatentCommand(this, Execution, DoWork, this->DefaultTimeout, this->bEnableSkipIfError)
		)
	);
}

void FEnhancedAutomationSpecBase::LatentBeforeEach(const EAsyncExecution                        Execution,
                                                   const FTimespan&                             Timeout,
                                                   const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->BeforeEach.Push(
		MakeShareable(new FAsyncMultiFrameLatentCommand(this, Execution, DoWork, Timeout, this->bEnableSkipIfError))
	);
}

void FEnhancedAutomationSpecBase::AfterEach(const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->AfterEach.Push(MakeShareable(new FSimpleBlockingCommand(this, DoWork)));
}

void FEnhancedAutomationSpecBase::AfterEach(const EAsyncExecution Execution, const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->AfterEach.Push(
		MakeShareable(new FAsyncCommand(this, Execution, DoWork, this->DefaultTimeout))
	);
}

void FEnhancedAutomationSpecBase::AfterEach(const EAsyncExecution    Execution,
                                            const FTimespan&         Timeout,
                                            const TFunction<void()>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->AfterEach.Push(MakeShareable(new FAsyncCommand(this, Execution, DoWork, Timeout)));
}

void FEnhancedAutomationSpecBase::LatentAfterEach(const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->AfterEach.Push(MakeShareable(new FMultiFrameLatentCommand(this, DoWork, this->DefaultTimeout)));
}

void FEnhancedAutomationSpecBase::LatentAfterEach(const FTimespan& Timeout,
                                                  const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->AfterEach.Push(MakeShareable(new FMultiFrameLatentCommand(this, DoWork, Timeout)));
}

void FEnhancedAutomationSpecBase::LatentAfterEach(const EAsyncExecution Execution,
                                                  const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->AfterEach.Push(
		MakeShareable(new FAsyncMultiFrameLatentCommand(this, Execution, DoWork, this->DefaultTimeout))
	);
}

void FEnhancedAutomationSpecBase::LatentAfterEach(const EAsyncExecution                        Execution,
                                                  const FTimespan&                             Timeout,
                                                  const TFunction<void(const FDoneDelegate&)>& DoWork)
{
	const TSharedRef<FSpecDefinitionScope> CurrentScope = this->DefinitionScopeStack.Last();

	CurrentScope->AfterEach.Push(MakeShareable(new FAsyncMultiFrameLatentCommand(this, Execution, DoWork, Timeout)));
}

void FEnhancedAutomationSpecBase::EnsureDefinitions() const
{
	if (!this->bHasBeenDefined)
	{
		const_cast<FEnhancedAutomationSpecBase*>(this)->Define();
		const_cast<FEnhancedAutomationSpecBase*>(this)->PostDefine();
	}
}

void FEnhancedAutomationSpecBase::PostDefine()
{
	TArray<TSharedRef<FSpecDefinitionScope>>     Stack;
	TArray<TSharedRef<IAutomationLatentCommand>> BeforeAll,
	                                             BeforeEach,
	                                             AfterEach;

	Stack.Push(RootDefinitionScope.ToSharedRef());

	while (Stack.Num() > 0)
	{
		const TSharedRef<FSpecDefinitionScope> Scope = Stack.Last();

		BeforeAll.Append(Scope->BeforeAll);
		BeforeEach.Append(Scope->BeforeEach);
		AfterEach.Append(Scope->AfterEach);

		for (auto ScopeIterator = Scope->It.CreateIterator(); ScopeIterator; ++ScopeIterator)
		{
			const TSharedRef<FSpecItDefinition> ItBlockScope = *ScopeIterator;
			TSharedRef<FSpec>                   Spec         = MakeShareable(new FSpec());
			FSpecVariableScope                  Variables;

			Spec->Id          = ItBlockScope->Id;
			Spec->Description = ItBlockScope->Description;
			Spec->Filename    = ItBlockScope->Filename;
			Spec->LineNumber  = ItBlockScope->LineNumber;

			Spec->Commands.Append(BeforeAll);
			Spec->Commands.Append(BeforeEach);
			Spec->Commands.Add(ItBlockScope->Command);

			// Iterate in reverse to evaluate AfterEach() from the inner-most scope outwards.
			for (int32 AfterEachIndex = AfterEach.Num() - 1; AfterEachIndex >= 0; --AfterEachIndex)
			{
				Spec->Commands.Add(AfterEach[AfterEachIndex]);
			}

			for (const auto& ScopeInStack : Stack)
			{
				Variables.Append(ScopeInStack->Variables);
			}

			Spec->Variables = Variables;

			check(!this->IdToSpecMap.Contains(Spec->Id));
			this->IdToSpecMap.Add(Spec->Id, Spec);
		}

		Scope->It.Empty();

		if (Scope->Children.Num() > 0)
		{
			// As long as we have deeper nested scopes, keep going.
			Stack.Append(Scope->Children);
			Scope->Children.Empty();
		}
		else
		{
			// We have no deeper scopes. Keep popping context off stacks until we reach an outer scope that has
			// children.
			while ((Stack.Num() > 0) && (Stack.Last()->Children.Num() == 0) && (Stack.Last()->It.Num() == 0))
			{
				const TSharedRef<FSpecDefinitionScope> PoppedScope        = Stack.Pop();
				const int32                            NumBeforeAllAdded  = PoppedScope->BeforeAll.Num(),
				                                       NumBeforeEachAdded = PoppedScope->BeforeEach.Num(),
				                                       NumAfterEachAdded  = PoppedScope->AfterEach.Num();

				if (NumBeforeAllAdded > 0)
				{
					// Remove all BeforeAll() blocks added by the current scope.
					BeforeAll.RemoveAt(BeforeAll.Num() - NumBeforeAllAdded, NumBeforeAllAdded);
				}

				if (NumBeforeEachAdded > 0)
				{
					// Remove all BeforeEach() blocks added by the current scope.
					BeforeEach.RemoveAt(BeforeEach.Num() - NumBeforeEachAdded, NumBeforeEachAdded);
				}

				if (NumAfterEachAdded > 0)
				{
					// Remove all AfterEach() blocks added by the current scope.
					AfterEach.RemoveAt(AfterEach.Num() - NumAfterEachAdded, NumAfterEachAdded);
				}
			}
		}
	}

	this->RootDefinitionScope.Reset();
	this->DefinitionScopeStack.Reset();

	this->bHasBeenDefined = true;
}

void FEnhancedAutomationSpecBase::Redefine()
{
	this->DescriptionStack.Empty();
	this->IdToSpecMap.Empty();
	this->RootDefinitionScope.Reset();
	this->DefinitionScopeStack.Empty();

	this->bHasBeenDefined = false;
}

FString FEnhancedAutomationSpecBase::GetId() const
{
	if (this->DescriptionStack.Last().EndsWith(TEXT("]")))
	{
		FString ItDescription = this->DescriptionStack.Last();

		ItDescription.RemoveAt(ItDescription.Len() - 1);

		int32 StartingBraceIndex = INDEX_NONE;

		if (ItDescription.FindLastChar(TEXT('['), StartingBraceIndex) &&
		    (StartingBraceIndex != (ItDescription.Len() - 1)))
		{
			FString CommandId = ItDescription.RightChop(StartingBraceIndex + 1);

			return CommandId;
		}
	}

	{
		FString CompleteId;

		for (auto DescriptionIterator = this->DescriptionStack.CreateConstIterator();
			 DescriptionIterator;
			 ++DescriptionIterator)
		{
			const FString CurrentDescription = *DescriptionIterator;

			if (CurrentDescription.IsEmpty())
			{
				continue;
			}

			if (CompleteId.IsEmpty())
			{
				CompleteId = CurrentDescription;
			}
			else if (FChar::IsWhitespace(CompleteId[CompleteId.Len() - 1]) ||
			         FChar::IsWhitespace(CurrentDescription[0]))
			{
				CompleteId = CompleteId + CurrentDescription;
			}
			else
			{
				CompleteId = FString::Printf(TEXT("%s %s"), *CompleteId, *CurrentDescription);
			}
		}

		return CompleteId;
	}
}

FString FEnhancedAutomationSpecBase::GetDescription() const
{
	FString CompleteDescription;

	for (auto DescriptionIterator = this->DescriptionStack.CreateConstIterator();
		 DescriptionIterator;
		 ++DescriptionIterator)
	{
		const FString CurrentDescription = *DescriptionIterator;

		if (CurrentDescription.IsEmpty())
		{
			continue;
		}

		if (CompleteDescription.IsEmpty())
		{
			CompleteDescription = CurrentDescription;
		}
		else if (FChar::IsWhitespace(CompleteDescription[CompleteDescription.Len() - 1]) ||
		         FChar::IsWhitespace(CurrentDescription[0]))
		{
			CompleteDescription = CompleteDescription + TEXT(".") + CurrentDescription;
		}
		else
		{
			CompleteDescription = FString::Printf(TEXT("%s.%s"), *CompleteDescription, *CurrentDescription);
		}
	}

	return CompleteDescription;
}

TSharedRef<TArray<FProgramCounterSymbolInfo>> FEnhancedAutomationSpecBase::GetCallStack()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FEnhancedAutomationSpecBase_GetStack);

	return MakeShared<TArray<FProgramCounterSymbolInfo>>(
		FAutomationTestFramework::NeedSkipStackWalk() ? SkipCallStackWalk() : CallStackWalk()
	);
}

TArray<FProgramCounterSymbolInfo> FEnhancedAutomationSpecBase::CallStackWalk()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FEnhancedAutomationSpecBase_StackWalk);
	LLM_SCOPE_BYNAME(TEXT("AutomationTest/Framework"));
	SAFE_GETSTACK(Stack, 3, 1);

	return Stack;
}

TArray<FProgramCounterSymbolInfo> FEnhancedAutomationSpecBase::SkipCallStackWalk()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FEnhancedAutomationSpecBase_SkipStackWalk);
	LLM_SCOPE_BYNAME(TEXT("AutomationTest/Framework"));

	TArray<FProgramCounterSymbolInfo> Stack;
	FProgramCounterSymbolInfo         First;

	TCString<ANSICHAR>::Strcpy(First.Filename, FProgramCounterSymbolInfo::MAX_NAME_LENGTH, "Unknown");

	First.LineNumber = 0;
	Stack.Add(First);

	return Stack;
}

void FEnhancedAutomationSpecBase::PushDescription(const FString& InDescription)
{
	LLM_SCOPE_BYNAME(TEXT("AutomationTest/Framework"));

	this->DescriptionStack.Add(InDescription);
}

void FEnhancedAutomationSpecBase::PopDescription()
{
	this->DescriptionStack.RemoveAt(this->DescriptionStack.Num() - 1);
}

TFunction<void()> FEnhancedAutomationSpecBase::CreateRunWorkOnceWrapper(const FSpecBlockHandle&  BlockHandle,
                                                                   const TFunction<void()>& DoWork) const
{
	return [=, this]
	{
		// Only allow this block to run once per test session per runner.
		if (!this->SuiteSessionState->HasBlockRun(BlockHandle))
		{
			DoWork();

			this->SuiteSessionState->MarkBlockAsRun(BlockHandle);
		}
	};
}

TFunction<void(const FDoneDelegate&)> FEnhancedAutomationSpecBase::CreateRunWorkOnceWrapper(
	const FSpecBlockHandle&                      BlockHandle,
	const TFunction<void(const FDoneDelegate&)>& DoWork) const
{
	return [=, this](const FDoneDelegate& DoneDelegate)
	{
		// Only allow this block to run once per test session on the current runner.
		if (!this->SuiteSessionState->HasBlockRun(BlockHandle))
		{
			DoWork(DoneDelegate);

			this->SuiteSessionState->MarkBlockAsRun(BlockHandle);
		}
	};
}

void FEnhancedAutomationSpecBase::RunSpec(const TSharedRef<FSpec>& SpecToRun)
{
	FAutomationTestFramework& AutomationTestFramework = FAutomationTestFramework::GetInstance();
	FSpecVariableScope&       Variables               = SpecToRun->Variables;

	for (const auto& [_, Variable] : Variables)
	{
		Variable->Reset();
	}

	this->VariablesInScope = Variables;

	for (int32 CommandIndex = 0; CommandIndex < SpecToRun->Commands.Num(); ++CommandIndex)
	{
		AutomationTestFramework.EnqueueLatentCommand(SpecToRun->Commands[CommandIndex]);
	}
}
