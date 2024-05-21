# Enhanced Automation Specs for Unreal Engine 5.3+

This is an upgraded version of Epic's
[Automation Spec](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-spec-in-unreal-engine)
framework with additional functionality that was inspired by [RSpec](https://rspec.info/), including support for
`BeforeAll` and `Let` blocks. The code in this repository is an MIT-licensed fork of code that was originally
implemented in the OpenPF2 project under an MPL license, to make this code easier to reuse in proprietary, commercial
projects.

## Why Does This Look Like a Rewrite of Automation Spec?
Epic did not design the original version of Automation Spec to be open for extension, so the version of Automation Spec
in OpenPF2 and this project is a cleaned up and refactored version of what's in Unreal Engine 5.3, plus the additional
functionality. This version of the framework also includes a fix to how stack traces in specs link back to code while in
the Session Frontend (it looks like Epic might have intentionally broken this functionality in their copy to improve
performance when loading large test suites, at the cost of making test failures significantly harder to debug).

## How to Use This

### Getting Started

1. Copy `Source/EnhancedAutomationSpecBase.cpp` and `Source/EnhancedAutomationSpecBase.h` into the appropriate part of
   your Unreal-based project (e.g., `Source/Private/Tests`).
2. Define specs using the macros described in Epic's
   [Automation Spec Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-spec-in-unreal-engine),
   making the following substitutions:
	- Use the `DEFINE_ENH_SPEC` macro in place of the `DEFINE_SPEC` macro in Epic's documentation.
	- Use the `BEGIN_DEFINE_ENH_SPEC` macro in place of the `BEGIN_DEFINE_SPEC` macro in Epic's documentation.
	- Use the `END_DEFINE_ENH_SPEC` macro in place of the `END_DEFINE_SPEC` macro in Epic's documentation.
3. Use the new features from this project as described in the section below. A full example of the new functionality is
   included in `Source/EnhancedAutomationSpecBase.spec.cpp`.

### Using `BeforeAll()`

`BeforeAll()` is similar to `before(:context)`/`before(:all)`
[from RSpec](https://rspec.info/features/3-13/rspec-core/hooks/before-and-after-hooks/). It is used to provide code that
must run before the first test expectation within a test scope (i.e., before the first `It()` of the scope in which it
has been defined).

#### Guidelines

- Multiple `BeforeAll()` blocks can be defined in the same scope.
- Each code block is executed from top to bottom before the first `It()` block within its scope.
- If a scope contains both `BeforeAll()` and `BeforeEach()` blocks, the `BeforeAll()` blocks will be evaluated before
  any BeforeEach() blocks are executed, including those inherited from outer scopes.
- `BeforeAll()` affects only the `Describe()` scope in which it is defined and its children.
- If there are `BeforeAll()` blocks in enclosing scopes, they run from top to bottom from outermost scope to innermost
  scope.
- When two adjacent scopes (call them "Scope A" and "Scope B") both use `BeforeAll()`, **there is no guarantee that all
  expectations in Scope A will be executed before expectations start being executed in Scope B.**
	- The only guarantee is that the `BeforeAll()` block in Scope A will be invoked before the expectations in Scope A
	  are invoked, and the `BeforeAll()` block in Scope B will be invoked before the expectations in Scope B are
	  invoked, but the `BeforeAll()` blocks in both Scope A and Scope B could very well be invoked before any
	  expectations of either scope are invoked.
	- This limitation is a result of how Unreal Engine evaluates and executes tests -- they are designed to be
	  distributed across any number of test runners, which means that there is no guarantee that one set of expectations
	  has finished before the next set is starting, since any expectation could be invoked concurrently with others on
	  separate runners.
	- For best results, do not manipulate variables in one `BeforeAll()` block that are also affected by other
	  `BeforeAll()` blocks. Instead, consider using `Let()` blocks for variables that need to be defined in outer scopes
	  and redefined in nested scopes.

#### Examples
```c++
BEGIN_DEFINE_ENH_SPEC(FBeforeAllDemoSpec,
                      "EnhancedUnrealSpecs.BeforeAllDemo",
                      EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
	int32 Test1RunCount(0);
	int32 Test2RunCount(0);
	
	Describe("BeforeAll()", [=, this]
	{
		Describe("when there are multiple expectations and no BeforeEach blocks", [=, this]
		{
			BeforeAll([=, this]
			{
				// This is just a simple example of initializing some state once before any expectation below has been 
				// run. You could use this for something more elaborate, like generating synthetic test data or
				// initializing a database connection. Just be sure that what you initialize here does not get
				// reinitialized by another `BeforeAll()` block because there is no guarantee that the tests within the
				// scope in which this `BeforeAll()` block will get invoked before a `BeforeAll()` block of an adjacent
				// scope.
				++Test1RunCount;
			});

			It("evaluates the `BeforeAll` at least once", [=, this]
			{
				TestEqual("Test1RunCount", this->Test1RunCount, 1);
			});

			It("evaluates the `BeforeAll` no more than once", [=, this]
			{
				// The count is unchanged from the prior expectation. (This assumes that both expectations
				// are being run on the same runner, as noted in the documentation below these code
				// examples.)
				TestEqual("Test1RunCount", this->Test1RunCount, 1);
			});
		});

		Describe("when there are multiple expectations and multiple BeforeEach blocks", [=, this]
		{
			BeforeEach([=, this]
			{
				// Second
				Test2RunCount *= 2;
			});
		
			BeforeAll([=, this]
			{
				// First
				++Test2RunCount;
			});
			
			BeforeEach([=, this]
			{
				// Third
				Test2RunCount += 1;
			});

			It("evaluates the `BeforeAll` before each `BeforeEach` block", [=, this]
			{
				// CORRECT   (First, Second, Third): (1 * 2) + 1 = 3
				// INCORRECT (Second, First, Third): (0 * 2) + 1 + 1 = 2
				// INCORRECT (Second, Third, First): (0 * 2) + 1 + 1 = 2
				// INCORRECT (Second and Third): (0 * 2) + 1 = 1
				TestEqual("Test2RunCount", this->Test2RunCount, 3);
			});
		});		
	});
END_DEFINE_ENH_SPEC(FBeforeAllDemoSpec)
```



### Using `Let` and `RedefineLet()`

`Let()` is similar to `let()` [from RSpec](https://rspec.info/features/3-12/rspec-core/helper-methods/let/). It is used 
to define a variable that is:
- **Lazily evaluated**, so it is only calculated in a particular expectation the first time its value is needed.
- **Cached across multiple references in the same expectation**, so it is only calculated once per expectation.
- **Not cached across expectations**, so it acts as if its value is reset at the start of each expectation.
- **Able to be redefined in a nested scope,** so common setup code can be defined in an outer scope and reused in inner
  scopes with different values.

A `RedefineLet()` block can be used in a nested scope to replace the logic used to calculate the value of a variable
that was declared with `Let()`. This method receives a reference to the original variable in case the new value should
depend on its original value (e.g., a list of items to which an additional element is being added, or a string to which
a single character is being appended).

Since C++ is not as forgiving as Ruby when it comes to variable types, we have defined (and strongly encourage the use
of) the `LET()` and `REDEFINE_LET()` macros as shorthand replacements for calling `Let()` and `RedefineLet()` directly.

Compare this:
```c++
	LET(SomeValue, FString, [], { return "SomeValue"; });
	REDEFINE_LET(SomeValue, FString, [], { return "NewValue"; });
```

To this:
```c++
	const TSpecVariable<FString> SomeValue = Let(TGeneratorFunc<FString>([] { return "SomeValue"; }));

	RedefineLet(
		SomeValue, 
		TGeneratorRedefineFunc<FString>([](const TSpecVariablePtr<FString>& Previous) { return "NewValue"; })
	);
```

#### Guidelines

- Instead of using a custom member field in an automation spec, consider using `Let()` to declare a variable instead.
  Unlike member fields that have to be initialized properly before expectations run, each variable declared with
  `Let()` does not have to be initialized before an expectation and is automatically cleared between expectation runs,
  preventing unwanted state from leaking across expectations.
- Multiple `Let()` and `RedefineLet()` blocks can be defined in the same scope.
- The code inside a `Let()` block is invoked the first time that the variable that block declares is dereferenced in an
  expectation. If an expectation never dereferences the value of a let variable that's in scope, the code block for the
  variable is never evaluated during that expectation.
- A `Let()` block can reference and build on the value of a prior `Let()` block as long as it lists the variable the
  earlier `Let()` block declared in its capture list (within the square brackets passed as the third variable to the
  `LET()` macro).
- A `RedefineLet()` block can reference and build on the value of the variable it is redefining. A reference to the
  prior value of the variable is passed into the generator function as the `Previous` parameter. Dereferencing the
  value of that parameter will cause the original value to be evaluated. This means that the same variable could be
  redefined multiple times within nested scopes, and the whole chain of values for that variable could be redefined in
  a chain.
- A variable defined by a `Let()` block can be referenced in a `BeforeEach()` block.
- Do not reference a variable defined by a `Let()` block in a `BeforeAll()` block.

#### Examples

These are taken from `EnhancedAutomationSpecBase.spec.cpp`:
```c++
BEGIN_DEFINE_ENH_SPEC(FLetDemoSpec,
                      "EnhancedUnrealSpecs.LetDemo",
                      EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)	
	Describe("Let()", [=, this]
	{
		Describe("when a variable is defined in a scope", [=, this]
		{
			LET(OuterValue1, TSharedPtr<FTestObject>, [],            { return MakeShared<FTestObject>("Outer"); });
			LET(OuterValue2, TSharedPtr<FTestObject>, [OuterValue1], { return *OuterValue1;                     });

			It("can supply the value via Get()", [=, this]
			{
				TestEqual("OuterValue1.Get().SomeValue", OuterValue1.Get().Get()->SomeValue, "Outer");
			});

			It("can supply the value via dereferencing", [=, this]
			{
				TestEqual("*OuterValue1->SomeValue", (*OuterValue1).Get()->SomeValue, "Outer");
			});

			It("can supply the value via arrow dereferencing", [=, this]
			{
				TestEqual("OuterValue1->SomeValue", OuterValue1->SomeValue, "Outer");
			});

			It("returns the same value every time during the same test", [=, this]
			{
				(*OuterValue1)->SomeValue = "Changed";

				TestEqual("OuterValue1", OuterValue1.Get().Get()->SomeValue, "Changed");
				TestEqual("OuterValue1", (*OuterValue1).Get()->SomeValue, "Changed");
				TestEqual("OuterValue1", OuterValue1->SomeValue, "Changed");
			});

			It("can provide values to variables after it in the scope", [=, this]
			{
				TestEqual("OuterValue2->SomeValue", OuterValue2->SomeValue, "Outer");
			});

			Describe("when a different variable is defined in a nested scope", [=, this]
			{
				LET(InnerValue, TSharedPtr<FTestObject>, [], { return MakeShared<FTestObject>("Inner"); });

				It("tracks the two variable separately in the current scope", [=, this]
				{
					TestEqual("*OuterValue1->SomeValue", OuterValue1->SomeValue, "Outer");
					TestEqual("*InnerValue->SomeValue", InnerValue->SomeValue, "Inner");
				});
			});

			Describe("when the same variable is redefined a second time in the same scope", [=, this]
			{
				Describe("when the redefinition does not reference the original value", [=, this]
				{
					LET(MyVariable, FString, [], { return "ABC"; });

					REDEFINE_LET(MyVariable, FString, [], { return "DEF"; });

					It("replaces the original value in the scope", [=, this]
					{
						TestEqual("MyVariable", *MyVariable, "DEF");
					});
				});

				Describe("when the redefinition references the original value", [=, this]
				{
					LET(MyVariable, FString, [], { return "ABC"; });

					REDEFINE_LET(MyVariable, FString, [], { return **Previous + "DEF"; });

					It("replaces the original value in the scope", [=, this]
					{
						TestEqual("MyVariable", *MyVariable, "ABCDEF");
					});
				});
			});

			Describe("when changing the value of a variable via its reference", [=, this]
			{
				LET(MyVariable, FString, [], { return "ABC"; });

				It("affects the value of the variable in the test that changes it", [=, this]
				{
					*MyVariable = "DEF";

					TestEqual("MyVariable", *MyVariable, "DEF");
				});

				It("does not affect the value of the variable in other tests", [=, this]
				{
					TestEqual("MyVariable", *MyVariable, "ABC");
				});
			});

			Describe("when the same variable is redefined in a nested scope", [=, this]
			{
				Describe("when the redefinition does not reference the original value", [=, this]
				{
					REDEFINE_LET(OuterValue1, TSharedPtr<FTestObject>, [], { return MakeShared<FTestObject>("Inner"); });

					It("replaces the original value in the scope", [=, this]
					{
						TestEqual("OuterValue1->SomeValue", OuterValue1->SomeValue, "Inner");
					});

					It("impacts the values of dependent variables in the outer scope", [=, this]
					{
						TestEqual("OuterValue2->SomeValue", OuterValue2->SomeValue, "Inner");
					});

					Describe("when the same variable is redefined a third time in an even deeper nested scope", [=, this]
					{
						Describe("when the second redefinition does not reference the original value", [=, this]
						{
							REDEFINE_LET(OuterValue1, TSharedPtr<FTestObject>, [], { return MakeShared<FTestObject>("DeepInner"); });

							It("replaces the original value in the scope", [=, this]
							{
								TestEqual("OuterValue1->SomeValue", OuterValue1->SomeValue, "DeepInner");
							});

							It("impacts the values of dependent variables in the outer scope", [=, this]
							{
								TestEqual("OuterValue2->SomeValue", OuterValue2->SomeValue, "DeepInner");
							});
						});

						Describe("when the second redefinition references the original value", [=, this]
						{
							REDEFINE_LET(
								OuterValue1,
								TSharedPtr<FTestObject>,
								[],
								{ return MakeShared<FTestObject>((*Previous)->SomeValue + "DeepInner"); }
							);

							It("replaces the original value in the scope", [=, this]
							{
								TestEqual("OuterValue1->SomeValue", OuterValue1->SomeValue, "InnerDeepInner");
							});

							It("impacts the values of dependent variables in the outer scope", [=, this]
							{
								TestEqual("OuterValue2->SomeValue", OuterValue2->SomeValue, "InnerDeepInner");
							});
						});
					});
				});

				Describe("when the redefinition references the original value", [=, this]
				{
					REDEFINE_LET(
						OuterValue1,
						TSharedPtr<FTestObject>,
						[],
						{ return MakeShared<FTestObject>((*Previous)->SomeValue + "Inner"); }
					);

					It("replaces the original value in the scope", [=, this]
					{
						TestEqual("OuterValue1->SomeValue", OuterValue1->SomeValue, "OuterInner");
					});

					It("impacts the values of dependent variables in the outer scope", [=, this]
					{
						TestEqual("OuterValue2->SomeValue", OuterValue2->SomeValue, "OuterInner");
					});

					Describe("when the same variable is redefined a third time in an even deeper nested scope", [=, this]
					{
						Describe("when the second redefinition does not reference the original value", [=, this]
						{
							REDEFINE_LET(
								OuterValue1,
								TSharedPtr<FTestObject>,
								[],
								{ return MakeShared<FTestObject>("DeepInner"); }
							);

							It("replaces the original value in the scope", [=, this]
							{
								TestEqual("OuterValue1->SomeValue", OuterValue1->SomeValue, "DeepInner");
							});

							It("impacts the values of dependent variables in the outer scope", [=, this]
							{
								TestEqual("OuterValue2->SomeValue", OuterValue2->SomeValue, "DeepInner");
							});
						});

						Describe("when the second redefinition references the original value", [=, this]
						{
							REDEFINE_LET(
								OuterValue1,
								TSharedPtr<FTestObject>,
								[],
								{ return MakeShared<FTestObject>((*Previous)->SomeValue + "DeepInner"); }
							);

							It("replaces the original value in the scope", [=, this]
							{
								TestEqual("OuterValue1->SomeValue", OuterValue1->SomeValue, "OuterInnerDeepInner");
							});

							It("impacts the values of dependent variables in the outer scope", [=, this]
							{
								TestEqual("OuterValue2->SomeValue", OuterValue2->SomeValue, "OuterInnerDeepInner");
							});
						});
					});
				});
			});
		});
	});

	Describe("BeforeEach() and Let()", [=, this]
	{
		Describe("when a variable is referenced by a BeforeEach() block", [=, this]
		{
			LET(Variable, FString, [], { return "ABC"; });

			BeforeEach([=, this]
			{
				*Variable += "XYZ";
			});

			It("provides a value to the BeforeEach() block the same as in a test", [=, this]
			{
				TestEqual("Variable", *Variable, "ABCXYZ");
			});

			Describe("when the variable is redefined in a nested scope", [=, this]
			{
				REDEFINE_LET(Variable, FString, [], { return "Inner"; });

				It("provides the redefined value to the outer BeforeEach() block", [=, this]
				{
					TestEqual("Variable", *Variable, "InnerXYZ");
				});
			});
		});
	});
END_DEFINE_ENH_SPEC(FLetDemoSpec)
```

## Licensing
As previously mentioned, the code in this repository is licensed under an MIT license for use in Unreal Engine projects.
As this code was based on code from Epic Games, it cannot be used outside an Unreal Engine project.

This project only re-licenses the automation spec components from OpenPF2 that have been explicitly included in this Git
repo. All other parts of OpenPF2 are subject to the conditions of the MPL + OGL licenses, as described in [the Core
OpenPF2 repository](https://github.com/OpenPF2/Core/blob/main/LICENSE.txt).
