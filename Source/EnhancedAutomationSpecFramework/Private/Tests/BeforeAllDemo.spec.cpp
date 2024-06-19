#include "EnhancedAutomationSpecBase.h"

BEGIN_DEFINE_ENH_SPEC(FBeforeAllDemoSpec,
                      "EnhancedUnrealSpecs.Demo.BeforeAll",
                      EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
	int32 Test1RunCount = 0;
	int32 Test2RunCount = 0;
END_DEFINE_ENH_SPEC(FBeforeAllDemoSpec)

void FBeforeAllDemoSpec::Define()
{
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
}
