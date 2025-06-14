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


#include <fstream>

#include "itkPointSetToListSampleAdaptor.h"
#include "itkWeightedCentroidKdTreeGenerator.h"
#include "itkKdTreeBasedKmeansEstimator.h"
#include "itkTestingMacros.h"

int
itkKdTreeBasedKmeansEstimatorTest(int argc, char * argv[])
{
  namespace stat = itk::Statistics;

  if (argc < 5)
  {
    std::cerr << "Missing Arguments" << std::endl;
    std::cerr << "Usage: " << std::endl;
    std::cerr << itkNameOfTestExecutableMacro(argv)
              << "inputFileName  bucketSize minStandardDeviation tolerancePercent useClusterLabels" << std::endl;
    return EXIT_FAILURE;
  }


  char *        dataFileName = argv[1];
  constexpr int dataSize = 2000;
  const int     bucketSize = std::stoi(argv[2]);
  const double  minStandardDeviation = std::stod(argv[3]);

  itk::Array<double> trueMeans(4);
  trueMeans[0] = 99.261;
  trueMeans[1] = 100.078;
  trueMeans[2] = 200.1;
  trueMeans[3] = 201.3;

  itk::Array<double> initialMeans(4);
  initialMeans[0] = 80.0;
  initialMeans[1] = 80.0;
  initialMeans[2] = 180.0;
  initialMeans[3] = 180.0;
  constexpr int maximumIteration = 200;

  /* Loading point data */
  using PointSetType = itk::PointSet<double, 2>;
  auto                                       pointSet = PointSetType::New();
  const PointSetType::PointsContainerPointer pointsContainer = PointSetType::PointsContainer::New();
  pointsContainer->Reserve(dataSize);
  pointSet->SetPoints(pointsContainer);

  PointSetType::PointsContainerIterator pIter = pointsContainer->Begin();
  PointSetType::PointType               point;
  double                                temp = NAN;
  std::ifstream                         dataStream(dataFileName);
  while (pIter != pointsContainer->End())
  {
    for (unsigned int i = 0; i < PointSetType::PointDimension; ++i)
    {
      dataStream >> temp;
      point[i] = temp;
    }
    pIter.Value() = point;
    ++pIter;
  }

  dataStream.close();

  /* Importing the point set to the sample */
  using DataSampleType = stat::PointSetToListSampleAdaptor<PointSetType>;

  auto sample = DataSampleType::New();

  sample->SetPointSet(pointSet);

  /* Creating k-d tree */
  using Generator = stat::WeightedCentroidKdTreeGenerator<DataSampleType>;
  auto generator = Generator::New();

  generator->SetSample(sample);
  generator->SetBucketSize(bucketSize);
  generator->GenerateData();

  /* Searching kmeans */
  using Estimator = stat::KdTreeBasedKmeansEstimator<Generator::KdTreeType>;
  auto estimator = Estimator::New();

  ITK_EXERCISE_BASIC_OBJECT_METHODS(estimator, KdTreeBasedKmeansEstimator, Object);


  // Set the initial means
  estimator->SetParameters(initialMeans);

  // Set the maximum iteration
  estimator->SetMaximumIteration(maximumIteration);
  ITK_TEST_SET_GET_VALUE(maximumIteration, estimator->GetMaximumIteration());

  estimator->SetKdTree(generator->GetOutput());

  // Set the centroid position change threshold
  estimator->SetCentroidPositionChangesThreshold(0.0);
  constexpr double tolerance = 0.1;
  if (itk::Math::abs(estimator->GetCentroidPositionChangesThreshold() - 0.0) > tolerance)
  {
    std::cerr << "Set/GetCentroidPositionChangesThreshold() " << std::endl;
    return EXIT_FAILURE;
  }

  auto useClusterLabels = static_cast<bool>(std::stoi(argv[5]));
  ITK_TEST_SET_GET_BOOLEAN(estimator, UseClusterLabels, useClusterLabels);

  estimator->StartOptimization();
  Estimator::ParametersType estimatedMeans = estimator->GetParameters();

  bool               passed = true;
  const unsigned int numberOfMeasurements = sample->GetMeasurementVectorSize();
  const unsigned int numberOfClasses = trueMeans.size() / numberOfMeasurements;
  for (unsigned int i = 0; i < numberOfClasses; ++i)
  {
    std::cout << "cluster[" << i << "] " << std::endl;
    double displacement = 0.0;
    std::cout << "    true mean :" << std::endl;
    std::cout << "        ";
    int index = numberOfMeasurements * i;
    for (unsigned int j = 0; j < numberOfMeasurements; ++j)
    {
      std::cout << trueMeans[index] << ' ';
      ++index;
    }
    std::cout << std::endl;
    std::cout << "    estimated mean :" << std::endl;
    std::cout << "        ";

    index = numberOfMeasurements * i;
    for (unsigned int j = 0; j < numberOfMeasurements; ++j)
    {
      std::cout << estimatedMeans[index] << ' ';
      temp = estimatedMeans[index] - trueMeans[index];
      ++index;
      displacement += (temp * temp);
    }
    std::cout << std::endl;
    displacement = std::sqrt(displacement);
    std::cout << "    Mean displacement: " << std::endl;
    std::cout << "        " << displacement << std::endl << std::endl;

    const double tolearancePercent = std::stod(argv[4]);

    // if the displacement of the estimates are within tolearancePercent% of
    // standardDeviation then we assume it is successful
    if (displacement > (minStandardDeviation * tolearancePercent))
    {
      std::cerr << "displacement is larger than tolerance ";
      std::cerr << minStandardDeviation * tolearancePercent << std::endl;
      passed = false;
    }
  }

  if (!passed)
  {
    std::cout << "Test failed." << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Test passed." << std::endl;
  return EXIT_SUCCESS;
}
