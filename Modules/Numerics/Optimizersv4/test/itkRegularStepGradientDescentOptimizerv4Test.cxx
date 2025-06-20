/*=========================================================================
 *
 *  Copyright NumFOCUS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         https://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#include "itkRegularStepGradientDescentOptimizerv4.h"
#include "itkGradientDescentOptimizerv4.h"
#include "itkMath.h"
#include "itkTestingMacros.h"

/**
 *  The objective function is the quadratic form:
 *
 *  1/2 x^T A x - b^T x
 *
 *  Where A is a matrix and b is a vector
 *  The system in this example is:
 *
 *     | 3  2 ||x|   | 2|   |0|
 *     | 2  6 ||y| + |-8| = |0|
 *
 *
 *   the solution is the vector | 2 -2 |
 *
 * \class RSGv4TestMetric
 */
class RSGv4TestMetric : public itk::ObjectToObjectMetricBase
{
public:
  using Self = RSGv4TestMetric;
  using Superclass = itk::ObjectToObjectMetricBase;
  using Pointer = itk::SmartPointer<Self>;
  using ConstPointer = itk::SmartPointer<const Self>;
  itkNewMacro(Self);

  enum
  {
    SpaceDimension = 2
  };

  using ParametersType = Superclass::ParametersType;
  using DerivativeType = Superclass::DerivativeType;
  using ParametersValueType = Superclass::ParametersValueType;
  using MeasureType = Superclass::MeasureType;


  RSGv4TestMetric()
  {
    m_Parameters.SetSize(SpaceDimension);
    m_Parameters.Fill(0);
  }

  void
  Initialize() override
  {}

  void
  GetDerivative(DerivativeType & derivative) const override
  {
    MeasureType value = NAN;
    GetValueAndDerivative(value, derivative);
  }

  void
  GetValueAndDerivative(MeasureType & value, DerivativeType & derivative) const override
  {
    if (derivative.Size() != 2)
    {
      derivative.SetSize(2);
    }

    const double x = m_Parameters[0];
    const double y = m_Parameters[1];

    std::cout << "GetValueAndDerivative( ";
    std::cout << x << ' ';
    std::cout << y << ") = ";

    value = 0.5 * (3 * x * x + 4 * x * y + 6 * y * y) - 2 * x + 8 * y;

    std::cout << "value: " << value << std::endl;

    //
    // The optimizer simply takes the derivative from the metric
    // and adds it to the transform after scaling. So instead of
    // setting a 'minimize' option in the gradient, we return
    // a minimizing derivative.
    //
    derivative[0] = -(3 * x + 2 * y - 2);
    derivative[1] = -(2 * x + 6 * y + 8);

    std::cout << "derivative: " << derivative << std::endl;
  }

  MeasureType
  GetValue() const override
  {
    return 0.0;
  }

  void
  UpdateTransformParameters(const DerivativeType & update, ParametersValueType factor) override
  {
    m_Parameters += update * factor;
  }

  unsigned int
  GetNumberOfParameters() const override
  {
    return SpaceDimension;
  }

  bool
  HasLocalSupport() const override
  {
    return false;
  }

  unsigned int
  GetNumberOfLocalParameters() const override
  {
    return SpaceDimension;
  }

  // These Set/Get methods are only needed for this test derivation that
  // isn't using a transform.
  void
  SetParameters(ParametersType & parameters) override
  {
    m_Parameters = parameters;
  }

  const ParametersType &
  GetParameters() const override
  {
    return m_Parameters;
  }

private:
  ParametersType m_Parameters;
};

template <typename OptimizerType>
int
RegularStepGradientDescentOptimizerv4TestHelper(
  itk::SizeValueType                                   numberOfIterations,
  bool                                                 doEstimateLearningRateAtEachIteration,
  bool                                                 doEstimateLearningRateOnce,
  typename OptimizerType::InternalComputationValueType relaxationFactor,
  typename OptimizerType::InternalComputationValueType minimumStepLength,
  typename OptimizerType::InternalComputationValueType gradientMagnitudeTolerance,
  typename OptimizerType::MeasureType                  currentLearningRateRelaxation)
{
  using ScalesType = typename OptimizerType::ScalesType;

  auto optimizer = OptimizerType::New();

  // Declaration of the metric
  auto metric = RSGv4TestMetric::New();

  optimizer->SetMetric(metric);

  using ParametersType = RSGv4TestMetric::ParametersType;

  const unsigned int spaceDimension = metric->GetNumberOfParameters();

  // We start not so far from | 2 -2 |
  ParametersType initialPosition(spaceDimension);
  initialPosition[0] = 100;
  initialPosition[1] = -100;
  metric->SetParameters(initialPosition);

  const typename OptimizerType::InternalComputationValueType learningRate = 100;
  optimizer->SetLearningRate(learningRate);

  optimizer->SetNumberOfIterations(numberOfIterations);

  optimizer->SetDoEstimateLearningRateAtEachIteration(doEstimateLearningRateAtEachIteration);
  optimizer->SetDoEstimateLearningRateOnce(doEstimateLearningRateOnce);

  ITK_TEST_SET_GET_VALUE(doEstimateLearningRateAtEachIteration, optimizer->GetDoEstimateLearningRateAtEachIteration());
  ITK_TEST_SET_GET_VALUE(doEstimateLearningRateOnce, optimizer->GetDoEstimateLearningRateOnce());

  optimizer->SetMinimumStepLength(minimumStepLength);
  ITK_TEST_SET_GET_VALUE(minimumStepLength, optimizer->GetMinimumStepLength());

  optimizer->SetGradientMagnitudeTolerance(gradientMagnitudeTolerance);
  ITK_TEST_SET_GET_VALUE(gradientMagnitudeTolerance, optimizer->GetGradientMagnitudeTolerance());

  optimizer->SetCurrentLearningRateRelaxation(currentLearningRateRelaxation);
  ITK_TEST_SET_GET_VALUE(currentLearningRateRelaxation, optimizer->GetCurrentLearningRateRelaxation());

  // Test exceptions
  ScalesType parametersScaleExcp(spaceDimension - 1);
  parametersScaleExcp.Fill(1.0);
  optimizer->SetScales(parametersScaleExcp);

  ITK_TRY_EXPECT_EXCEPTION(optimizer->StartOptimization());

  ScalesType parametersScale(spaceDimension);
  parametersScale.Fill(1.0);
  optimizer->SetScales(parametersScale);

  optimizer->SetRelaxationFactor(relaxationFactor);
  ITK_TEST_SET_GET_VALUE(relaxationFactor, optimizer->GetRelaxationFactor());

  std::cout << "CurrentPosition before optimization: " << optimizer->GetMetric()->GetParameters() << std::endl;

  ITK_TRY_EXPECT_NO_EXCEPTION(optimizer->StartOptimization());


  std::cout << "CurrentPosition after optimization: " << optimizer->GetMetric()->GetParameters() << std::endl;
  std::cout << "Stop Condition: " << optimizer->GetStopConditionDescription() << std::endl;


  if (optimizer->GetCurrentIteration() > 0)
  {
    std::cerr << "The optimizer is running iterations despite of ";
    std::cerr << "having a maximum number of iterations set to zero" << std::endl;
    return EXIT_FAILURE;
  }

  ParametersType finalPosition = optimizer->GetMetric()->GetParameters();
  std::cout << "Solution        = (";
  std::cout << finalPosition[0] << ',';
  std::cout << finalPosition[1] << ')' << std::endl;

  //
  // Check results to see if it is within range
  //
  bool             pass = true;
  constexpr double trueParameters[2] = { 2, -2 };
  for (unsigned int j = 0; j < 2; ++j)
  {
    if (itk::Math::FloatAlmostEqual(finalPosition[j], trueParameters[j]))
    {
      pass = false;
    }
  }

  if (!pass)
  {
    std::cout << "Test failed." << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}


int
itkRegularStepGradientDescentOptimizerv4Test(int, char *[])
{

  using OptimizerType = itk::RegularStepGradientDescentOptimizerv4<double>;

  auto itkOptimizer = OptimizerType::New();

  ITK_EXERCISE_BASIC_OBJECT_METHODS(
    itkOptimizer, RegularStepGradientDescentOptimizerv4, GradientDescentOptimizerv4Template);

  bool testStatus = EXIT_SUCCESS;


  constexpr bool doEstimateLearningRateAtEachIteration = false;
  constexpr bool doEstimateLearningRateOnce = false;

  constexpr itk::SizeValueType numberOfIterations = 900;

  constexpr OptimizerType::InternalComputationValueType relaxationFactor = 0.5;
  constexpr OptimizerType::InternalComputationValueType minimumStepLength = 1e-6;
  constexpr OptimizerType::InternalComputationValueType gradientMagnitudeTolerance = 1e-6;
  constexpr OptimizerType::MeasureType                  currentLearningRateRelaxation = 0;

  testStatus = RegularStepGradientDescentOptimizerv4TestHelper<OptimizerType>(numberOfIterations,
                                                                              doEstimateLearningRateAtEachIteration,
                                                                              doEstimateLearningRateOnce,
                                                                              relaxationFactor,
                                                                              minimumStepLength,
                                                                              gradientMagnitudeTolerance,
                                                                              currentLearningRateRelaxation);


  // Run now with different learning rate estimation frequencies
  std::cout << "\nRun test with a different learning rate estimation frequencies:"
               " estimate learning rate at each iteration: true; "
               " estimate learning rate once: false."
            << std::endl;
  {
    constexpr bool doEstimateLearningRateAtEachIteration2 = true;
    constexpr bool doEstimateLearningRateOnce2 = false;
    testStatus = RegularStepGradientDescentOptimizerv4TestHelper<OptimizerType>(numberOfIterations,
                                                                                doEstimateLearningRateAtEachIteration2,
                                                                                doEstimateLearningRateOnce2,
                                                                                relaxationFactor,
                                                                                minimumStepLength,
                                                                                gradientMagnitudeTolerance,
                                                                                currentLearningRateRelaxation);
  }

  // Run now with different learning rate estimation frequencies
  std::cout << "\nRun test with a different learning rate estimation frequencies:"
               " estimate learning rate at each iteration: false; "
               " estimate learning rate once: true."
            << std::endl;
  {
    constexpr bool doEstimateLearningRateAtEachIteration3 = false;
    constexpr bool doEstimateLearningRateOnce3 = true;

    testStatus = RegularStepGradientDescentOptimizerv4TestHelper<OptimizerType>(numberOfIterations,
                                                                                doEstimateLearningRateAtEachIteration3,
                                                                                doEstimateLearningRateOnce3,
                                                                                relaxationFactor,
                                                                                minimumStepLength,
                                                                                gradientMagnitudeTolerance,
                                                                                currentLearningRateRelaxation);
  }

  // Run now with different learning rate estimation frequencies
  std::cout << "\nRun test with a different learning rate estimation frequencies:"
               " estimate learning rate at each iteration: true; "
               " estimate learning rate once: true."
            << std::endl;
  {
    constexpr bool doEstimateLearningRateAtEachIteration4 = true;
    constexpr bool doEstimateLearningRateOnce4 = true;

    testStatus = RegularStepGradientDescentOptimizerv4TestHelper<OptimizerType>(numberOfIterations,
                                                                                doEstimateLearningRateAtEachIteration4,
                                                                                doEstimateLearningRateOnce4,
                                                                                relaxationFactor,
                                                                                minimumStepLength,
                                                                                gradientMagnitudeTolerance,
                                                                                currentLearningRateRelaxation);
  }

  // Run now with a different relaxation factor
  std::cout << "\nRun test with a different relaxation factor: 0.8, instead of default value: 0.5." << std::endl;
  {
    constexpr OptimizerType::InternalComputationValueType relaxationFactor2 = 0.8;

    testStatus = RegularStepGradientDescentOptimizerv4TestHelper<OptimizerType>(numberOfIterations,
                                                                                doEstimateLearningRateAtEachIteration,
                                                                                doEstimateLearningRateOnce,
                                                                                relaxationFactor2,
                                                                                minimumStepLength,
                                                                                gradientMagnitudeTolerance,
                                                                                currentLearningRateRelaxation);
  }


  // Verify that the optimizer doesn't run if the number of iterations is set to zero.
  std::cout << "\nCheck the optimizer when number of iterations is set to zero:" << std::endl;
  {
    constexpr itk::SizeValueType numberOfIterations2 = 0;

    testStatus = RegularStepGradientDescentOptimizerv4TestHelper<OptimizerType>(numberOfIterations2,
                                                                                doEstimateLearningRateAtEachIteration,
                                                                                doEstimateLearningRateOnce,
                                                                                relaxationFactor,
                                                                                minimumStepLength,
                                                                                gradientMagnitudeTolerance,
                                                                                currentLearningRateRelaxation);
  }

  //
  // Test the Exception if the GradientMagnitudeTolerance is set to a negative value
  //
  std::cout << "\nTest the Exception if the GradientMagnitudeTolerance is set to a negative value:" << std::endl;
  {
    constexpr OptimizerType::InternalComputationValueType gradientMagnitudeTolerance2 = -1.0;
    const bool                                            expectedExceptionReceived =
      RegularStepGradientDescentOptimizerv4TestHelper<OptimizerType>(numberOfIterations,
                                                                     doEstimateLearningRateAtEachIteration,
                                                                     doEstimateLearningRateOnce,
                                                                     relaxationFactor,
                                                                     minimumStepLength,
                                                                     gradientMagnitudeTolerance2,
                                                                     currentLearningRateRelaxation);

    if (!expectedExceptionReceived)
    {
      std::cerr << "Failure to produce an exception when";
      std::cerr << " the GradientMagnitudeTolerance is negative " << std::endl;
      std::cerr << "TEST FAILED !" << std::endl;
      testStatus = EXIT_FAILURE;
    }
  }

  //
  // Test the Exception if the RelaxationFactor is set to a negative value.
  //
  std::cout << "\nTest the Exception if the RelaxationFactor is set to a negative value:" << std::endl;
  {
    constexpr itk::SizeValueType                          numberOfIterations3 = 100;
    constexpr OptimizerType::InternalComputationValueType relaxationFactor3 = -1.0;
    constexpr OptimizerType::InternalComputationValueType gradientMagnitudeTolerance3 = 0.01;
    const bool                                            expectedExceptionReceived =
      RegularStepGradientDescentOptimizerv4TestHelper<OptimizerType>(numberOfIterations3,
                                                                     doEstimateLearningRateAtEachIteration,
                                                                     doEstimateLearningRateOnce,
                                                                     relaxationFactor3,
                                                                     minimumStepLength,
                                                                     gradientMagnitudeTolerance3,
                                                                     currentLearningRateRelaxation);
    if (!expectedExceptionReceived)
    {
      std::cerr << "Failure to produce an exception when";
      std::cerr << " the RelaxationFactor is negative " << std::endl;
      std::cerr << "TEST FAILED !" << std::endl;
      testStatus = EXIT_FAILURE;
    }
  }

  //
  // Test the Exception if the RelaxationFactor is set to a value larger than one.
  //
  std::cout << "\nTest the Exception if the RelaxationFactor is set to a value larger than one:" << std::endl;
  {
    constexpr itk::SizeValueType                          numberOfIterations4 = 100;
    constexpr OptimizerType::InternalComputationValueType relaxationFactor4 = 1.1;
    constexpr OptimizerType::InternalComputationValueType gradientMagnitudeTolerance4 = 0.01;
    const bool                                            expectedExceptionReceived =
      RegularStepGradientDescentOptimizerv4TestHelper<OptimizerType>(numberOfIterations4,
                                                                     doEstimateLearningRateAtEachIteration,
                                                                     doEstimateLearningRateOnce,
                                                                     relaxationFactor4,
                                                                     minimumStepLength,
                                                                     gradientMagnitudeTolerance4,
                                                                     currentLearningRateRelaxation);

    if (!expectedExceptionReceived)
    {
      std::cerr << "Failure to produce an exception when";
      std::cerr << " the RelaxationFactor is larger than one " << std::endl;
      std::cerr << "TEST FAILED !" << std::endl;
      testStatus = EXIT_FAILURE;
    }
  }

  if (!testStatus)
  {
    std::cout << "Test finished." << std::endl;
  }
  else
  {
    std::cout << "TEST FAILED!" << std::endl;
  }

  return testStatus;
}
