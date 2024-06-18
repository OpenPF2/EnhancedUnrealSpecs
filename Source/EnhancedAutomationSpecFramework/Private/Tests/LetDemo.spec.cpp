#include "EnhancedAutomationSpecBase.h"

DEFINE_ENH_SPEC(FLetDemoSpec,
                "EnhancedUnrealSpecs.Demo.Let",
                EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask);

struct FTestObject
{
	FString SomeValue;

	explicit FTestObject(const FString& SomeValue) : SomeValue(SomeValue)
	{
	}
};

void FLetDemoSpec::Define()
{
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
}
