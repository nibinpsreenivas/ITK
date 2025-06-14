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
#ifndef itkBlockMatchingImageFilter_hxx
#define itkBlockMatchingImageFilter_hxx

#include "itkImageRegionConstIterator.h"
#include "itkConstNeighborhoodIterator.h"
#include <limits>
#include "itkMultiThreaderBase.h"
#include "itkMakeUniqueForOverwrite.h"


namespace itk
{
template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::
  BlockMatchingImageFilter()
{
  // defaults
  this->m_BlockRadius.Fill(2);
  this->m_SearchRadius.Fill(3);

  // Make the outputs (Displacements, Similarities).
  ProcessObject::MakeRequiredOutputs(*this, 2);

  // all inputs are required
  this->SetPrimaryInputName("FeaturePoints");
  this->AddRequiredInputName("FixedImage");
  this->AddRequiredInputName("MovingImage");
}

template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
void
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::PrintSelf(
  std::ostream & os,
  Indent         indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "BlockRadius: " << static_cast<typename NumericTraits<ImageSizeType>::PrintType>(m_BlockRadius)
     << std::endl;
  os << indent << "SearchRadius: " << static_cast<typename NumericTraits<ImageSizeType>::PrintType>(m_SearchRadius)
     << std::endl;
  os << indent << "PointsCount: " << static_cast<typename NumericTraits<SizeValueType>::PrintType>(m_PointsCount)
     << std::endl;

  os << indent << "DisplacementsVectorsArray: ";
  if (m_DisplacementsVectorsArray != nullptr)
  {
    os << *m_DisplacementsVectorsArray.get() << std::endl;
  }
  else
  {
    os << "(null)" << std::endl;
  }

  os << indent << "SimilaritiesValuesArray: ";
  if (m_SimilaritiesValuesArray != nullptr)
  {
    os << *m_SimilaritiesValuesArray.get() << std::endl;
  }
  else
  {
    os << "(null)" << std::endl;
  }
}

template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
void
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::
  GenerateOutputInformation()
{
  // We use the constructor defaults for all regions.
}

template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
void
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::
  EnlargeOutputRequestedRegion(DataObject * output)
{
  output->SetRequestedRegionToLargestPossibleRegion();
}

template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
void
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::GenerateData()
{
  // Call a method that can be overridden by a subclass to perform
  // some calculations prior to splitting the main computations into
  // separate threads
  this->BeforeThreadedGenerateData();

  // Set up the multithreaded processing
  ThreadStruct str;
  str.Filter = this;

  this->GetMultiThreader()->SetNumberOfWorkUnits(this->GetNumberOfWorkUnits());
  this->GetMultiThreader()->SetSingleMethodAndExecute(this->ThreaderCallback, &str);

  // Call a method that can be overridden by a subclass to perform
  // some calculations after all the threads have completed
  this->AfterThreadedGenerateData();
}

template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
DataObject::Pointer
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::MakeOutput(
  ProcessObject::DataObjectPointerArraySizeType idx)
{
  switch (idx)
  {
    case 0:
    {
      return DisplacementsType::New().GetPointer();
    }
    break;

    case 1:
    {
      return SimilaritiesType::New().GetPointer();
    }
    break;
  }
  itkExceptionMacro("Bad output index " << idx);
}


template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
void
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::
  BeforeThreadedGenerateData()
{
  this->m_PointsCount = SizeValueType{};
  const FeaturePointsConstPointer featurePoints = this->GetFeaturePoints();
  if (featurePoints)
  {
    this->m_PointsCount = featurePoints->GetNumberOfPoints();
  }

  if (this->m_PointsCount < 1)
  {
    itkExceptionMacro("Invalid number of feature points: " << this->m_PointsCount << '.');
  }

  this->m_DisplacementsVectorsArray = make_unique_for_overwrite<DisplacementsVector[]>(this->m_PointsCount);
  this->m_SimilaritiesValuesArray = make_unique_for_overwrite<SimilaritiesValue[]>(this->m_PointsCount);
}

template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
void
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::
  AfterThreadedGenerateData()
{
  const FeaturePointsConstPointer featurePoints = this->GetFeaturePoints();
  if (featurePoints)
  {
    const typename FeaturePointsType::PointsContainer * points = featurePoints->GetPoints();

    const DisplacementsPointer displacements = this->GetDisplacements();

    using DisplacementsPointsContainerPointerType = typename DisplacementsType::PointsContainerPointer;
    using DisplacementsPointsContainerType = typename DisplacementsType::PointsContainer;
    const DisplacementsPointsContainerPointerType displacementsPoints = DisplacementsPointsContainerType::New();

    using DisplacementsPointDataContainerPointerType = typename DisplacementsType::PointDataContainerPointer;
    using DisplacementsPointDataContainerType = typename DisplacementsType::PointDataContainer;
    const DisplacementsPointDataContainerPointerType displacementsData = DisplacementsPointDataContainerType::New();

    const SimilaritiesPointer similarities = this->GetSimilarities();

    using SimilaritiesPointsContainerPointerType = typename SimilaritiesType::PointsContainerPointer;
    using SimilaritiesPointsContainerType = typename SimilaritiesType::PointsContainer;
    const SimilaritiesPointsContainerPointerType similaritiesPoints = SimilaritiesPointsContainerType::New();

    using SimilaritiesPointDataContainerPointerType = typename SimilaritiesType::PointDataContainerPointer;
    using SimilaritiesPointDataContainerType = typename SimilaritiesType::PointDataContainer;
    const SimilaritiesPointDataContainerPointerType similaritiesData = SimilaritiesPointDataContainerType::New();

    // insert displacements and similarities
    for (SizeValueType i = 0; i < this->m_PointsCount; ++i)
    {
      displacementsPoints->InsertElement(i, points->GetElement(i));
      similaritiesPoints->InsertElement(i, points->GetElement(i));
      displacementsData->InsertElement(i, this->m_DisplacementsVectorsArray[i]);
      similaritiesData->InsertElement(i, this->m_SimilaritiesValuesArray[i]);
    }

    displacements->SetPoints(displacementsPoints);
    displacements->SetPointData(displacementsData);
    similarities->SetPoints(similaritiesPoints);
    similarities->SetPointData(similaritiesData);
  }

  // clean up
  m_DisplacementsVectorsArray.reset();
  m_SimilaritiesValuesArray.reset();
}

template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
ITK_THREAD_RETURN_FUNCTION_CALL_CONVENTION
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::ThreaderCallback(
  void * arg)
{
  auto *             str = (ThreadStruct *)((static_cast<MultiThreaderBase::WorkUnitInfo *>(arg))->UserData);
  const ThreadIdType workUnitID = (static_cast<MultiThreaderBase::WorkUnitInfo *>(arg))->WorkUnitID;

  str->Filter->ThreadedGenerateData(workUnitID);

  return ITK_THREAD_RETURN_DEFAULT_VALUE;
}

template <typename TFixedImage,
          typename TMovingImage,
          typename TFeatures,
          typename TDisplacements,
          typename TSimilarities>
void
BlockMatchingImageFilter<TFixedImage, TMovingImage, TFeatures, TDisplacements, TSimilarities>::ThreadedGenerateData(
  ThreadIdType threadId)
{
  const FixedImageConstPointer    fixedImage = this->GetFixedImage();
  const MovingImageConstPointer   movingImage = this->GetMovingImage();
  const FeaturePointsConstPointer featurePoints = this->GetFeaturePoints();

  const SizeValueType workUnitCount = this->GetNumberOfWorkUnits();

  // compute first point and number of points (count) for this thread
  SizeValueType       count = m_PointsCount / workUnitCount;
  const SizeValueType first = threadId * count;
  if (threadId + 1 == workUnitCount) // last thread
  {
    count += this->m_PointsCount % workUnitCount;
  }

  // start constructing window region and center region (single voxel)
  ImageRegionType window;
  ImageRegionType center;
  auto            windowSize = ImageSizeType::Filled(1);
  center.SetSize(windowSize); // size of center region is 1
  windowSize += m_SearchRadius + m_SearchRadius;
  window.SetSize(windowSize); // size of window region is 1+2*m_BlockHalfWindow

  // start constructing block iterator
  SizeValueType numberOfVoxelInBlock = 1;
  for (unsigned int i = 0; i < ImageSizeType::Dimension; ++i)
  {
    numberOfVoxelInBlock *= m_BlockRadius[i] + 1 + m_BlockRadius[i];
  }

  // loop thru feature points
  for (SizeValueType idx = first, last = first + count; idx < last; ++idx)
  {
    const FeaturePointsPhysicalCoordinates originalLocation = featurePoints->GetPoint(idx);
    const auto                             fixedIndex = fixedImage->TransformPhysicalPointToIndex(originalLocation);
    const auto                             movingIndex = movingImage->TransformPhysicalPointToIndex(originalLocation);

    // the block is selected for a minimum similarity metric
    SimilaritiesValue similarity{};

    // New point location
    DisplacementsVector displacement;

    // set centers of window and center regions to current location
    const ImageIndexType start = fixedIndex - this->m_SearchRadius;
    window.SetIndex(start);
    center.SetIndex(movingIndex);

    // iterate over neighborhoods in region window, for each neighborhood: iterate over voxels in blockRadius
    ConstNeighborhoodIterator<FixedImageType> windowIterator(m_BlockRadius, fixedImage, window);

    // iterate over voxels in neighborhood of current feature point
    ConstNeighborhoodIterator<MovingImageType> centerIterator(m_BlockRadius, movingImage, center);
    centerIterator.GoToBegin();

    // iterate over neighborhoods in region window
    for (windowIterator.GoToBegin(); !windowIterator.IsAtEnd(); ++windowIterator)
    {
      SimilaritiesValue fixedSum{};
      SimilaritiesValue fixedSumOfSquares{};
      SimilaritiesValue movingSum{};
      SimilaritiesValue movingSumOfSquares{};
      SimilaritiesValue covariance{};

      // iterate over voxels in blockRadius
      for (SizeValueType i = 0; i < numberOfVoxelInBlock; ++i) // windowIterator.Size() == numberOfVoxelInBlock
      {
        const SimilaritiesValue fixedValue = windowIterator.GetPixel(i);
        const SimilaritiesValue movingValue = centerIterator.GetPixel(i);
        movingSum += movingValue;
        fixedSum += fixedValue;
        movingSumOfSquares += movingValue * movingValue;
        fixedSumOfSquares += fixedValue * fixedValue;
        covariance += fixedValue * movingValue;
      }
      const SimilaritiesValue fixedMean = fixedSum / numberOfVoxelInBlock;
      const SimilaritiesValue movingMean = movingSum / numberOfVoxelInBlock;
      const SimilaritiesValue fixedVariance = fixedSumOfSquares - numberOfVoxelInBlock * fixedMean * fixedMean;
      const SimilaritiesValue movingVariance = movingSumOfSquares - numberOfVoxelInBlock * movingMean * movingMean;
      covariance -= numberOfVoxelInBlock * fixedMean * movingMean;

      SimilaritiesValue sim{};
      if ((fixedVariance * movingVariance) != 0.0)
      {
        sim = (covariance * covariance) / (fixedVariance * movingVariance);
      }

      if (sim >= similarity)
      {
        FeaturePointsPhysicalCoordinates newLocation;
        fixedImage->TransformIndexToPhysicalPoint(windowIterator.GetIndex(), newLocation);
        displacement = newLocation - originalLocation;
        similarity = sim;
      }
    }
    this->m_DisplacementsVectorsArray[idx] = displacement;
    this->m_SimilaritiesValuesArray[idx] = similarity;
  }
}

} // end namespace itk

#endif
