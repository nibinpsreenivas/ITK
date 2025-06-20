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
#ifndef itkMeanSquaresImageToImageMetric_hxx
#define itkMeanSquaresImageToImageMetric_hxx

#include "itkCovariantVector.h"
#include "itkImageRegionIterator.h"
#include "itkImageIterator.h"
#include "itkMath.h"
#include "itkMakeUniqueForOverwrite.h"

namespace itk
{

template <typename TFixedImage, typename TMovingImage>
MeanSquaresImageToImageMetric<TFixedImage, TMovingImage>::MeanSquaresImageToImageMetric()
{
  this->SetComputeGradient(true);

  this->m_WithinThreadPreProcess = false;
  this->m_WithinThreadPostProcess = false;

  //  For backward compatibility, the default behavior is to use all the pixels
  //  in the fixed image.
  //  This should be fixed in ITKv4 so that this metric behaves as the others.
  this->SetUseAllPixels(true);
}

template <typename TFixedImage, typename TMovingImage>
void
MeanSquaresImageToImageMetric<TFixedImage, TMovingImage>::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "PerThread: ";
  if (m_PerThread.get() != nullptr)
  {
    os << m_PerThread.get() << std::endl;
  }
  else
  {
    os << "(null)" << std::endl;
  }
}

template <typename TFixedImage, typename TMovingImage>
void
MeanSquaresImageToImageMetric<TFixedImage, TMovingImage>::Initialize()
{
  this->Superclass::Initialize();
  this->Superclass::MultiThreadingInitialize();

  m_PerThread = make_unique_for_overwrite<AlignedPerThreadType[]>(this->m_NumberOfWorkUnits);

  for (ThreadIdType workUnitID = 0; workUnitID < this->m_NumberOfWorkUnits; ++workUnitID)
  {
    m_PerThread[workUnitID].m_MSEDerivative.SetSize(this->m_NumberOfParameters);
  }
}

template <typename TFixedImage, typename TMovingImage>
inline bool
MeanSquaresImageToImageMetric<TFixedImage, TMovingImage>::GetValueThreadProcessSample(
  ThreadIdType                 threadId,
  SizeValueType                fixedImageSample,
  const MovingImagePointType & itkNotUsed(mappedPoint),
  double                       movingImageValue) const
{
  const double diff = movingImageValue - this->m_FixedImageSamples[fixedImageSample].value;

  m_PerThread[threadId].m_MSE += diff * diff;

  return true;
}

template <typename TFixedImage, typename TMovingImage>
auto
MeanSquaresImageToImageMetric<TFixedImage, TMovingImage>::GetValue(const ParametersType & parameters) const
  -> MeasureType
{
  itkDebugMacro("GetValue( " << parameters << " ) ");

  if (!this->m_FixedImage)
  {
    itkExceptionMacro("Fixed image has not been assigned");
  }

  for (unsigned int i = 0; i < this->m_NumberOfWorkUnits; ++i)
  {
    m_PerThread[i].m_MSE = MeasureType{};
  }

  // Set up the parameters in the transform
  this->m_Transform->SetParameters(parameters);

  // MUST BE CALLED TO INITIATE PROCESSING
  this->GetValueMultiThreadedInitiate();

  itkDebugMacro("Ratio of voxels mapping into moving image buffer: " << this->m_NumberOfPixelsCounted << " / "
                                                                     << this->m_NumberOfFixedImageSamples << std::endl);

  if (this->m_NumberOfPixelsCounted < this->m_NumberOfFixedImageSamples / 4)
  {
    itkExceptionMacro("Too many samples map outside moving image buffer: "
                      << this->m_NumberOfPixelsCounted << " / " << this->m_NumberOfFixedImageSamples << std::endl);
  }

  double mse = m_PerThread[0].m_MSE;
  for (unsigned int t = 1; t < this->m_NumberOfWorkUnits; ++t)
  {
    mse += m_PerThread[t].m_MSE;
  }
  mse /= this->m_NumberOfPixelsCounted;

  return mse;
}

template <typename TFixedImage, typename TMovingImage>
inline bool
MeanSquaresImageToImageMetric<TFixedImage, TMovingImage>::GetValueAndDerivativeThreadProcessSample(
  ThreadIdType                 threadId,
  SizeValueType                fixedImageSample,
  const MovingImagePointType & itkNotUsed(mappedPoint),
  double                       movingImageValue,
  const ImageDerivativesType & movingImageGradientValue) const
{
  const double diff = movingImageValue - this->m_FixedImageSamples[fixedImageSample].value;

  AlignedPerThreadType & threadS = m_PerThread[threadId];

  threadS.m_MSE += diff * diff;

  const FixedImagePointType fixedImagePoint = this->m_FixedImageSamples[fixedImageSample].point;

  // Need to use one of the threader transforms if we're
  // not in thread 0.
  //
  // Use a raw pointer here to avoid the overhead of smart pointers.
  // For instance, Register and UnRegister have mutex locks around
  // the reference counts.
  TransformType * transform = this->m_Transform;

  if (threadId > 0)
  {
    transform = this->m_ThreaderTransform[threadId - 1];
  }

  if (this->m_BSplineTransform && this->m_UseCachingOfBSplineWeights)
  {
    // using pre-computed weights and indexes to calculate only non zero elements of the derivative
    for (unsigned int w = 0; w < this->m_NumBSplineWeights; ++w)
    {
      const auto precomputedIndex = this->m_BSplineTransformIndicesArray[fixedImageSample][w];
      const auto precomputedWeight = this->m_BSplineTransformWeightsArray[fixedImageSample][w];
      for (unsigned int dim = 0; dim < MovingImageDimension; ++dim)
      {
        const int par = precomputedIndex + this->m_BSplineParametersOffset[dim];
        threadS.m_MSEDerivative[par] += 2.0 * diff * precomputedWeight * movingImageGradientValue[dim];
      }
    }
  }
  else
  {
    // Use generic transform to compute Jacobian at the unmapped (fixed image) point.
    transform->ComputeJacobianWithRespectToParameters(fixedImagePoint, threadS.m_Jacobian);
    for (unsigned int par = 0; par < this->m_NumberOfParameters; ++par)
    {
      double sum = 0.0;
      for (unsigned int dim = 0; dim < MovingImageDimension; ++dim)
      {
        sum += 2.0 * diff * threadS.m_Jacobian(dim, par) * movingImageGradientValue[dim];
      }
      threadS.m_MSEDerivative[par] += sum;
    }
  }

  return true;
}

template <typename TFixedImage, typename TMovingImage>
void
MeanSquaresImageToImageMetric<TFixedImage, TMovingImage>::GetValueAndDerivative(const ParametersType & parameters,
                                                                                MeasureType &          value,
                                                                                DerivativeType &       derivative) const
{
  if (!this->m_FixedImage)
  {
    itkExceptionMacro("Fixed image has not been assigned");
  }

  // Set up the parameters in the transform
  this->m_Transform->SetParameters(parameters);

  // Reset the joint pdfs to zero
  for (unsigned int i = 0; i < this->m_NumberOfWorkUnits; ++i)
  {
    m_PerThread[i].m_MSE = MeasureType{};
  }

  // Set output values to zero
  if (derivative.GetSize() != this->m_NumberOfParameters)
  {
    derivative = DerivativeType(this->m_NumberOfParameters);
  }
  memset(derivative.data_block(), 0, this->m_NumberOfParameters * sizeof(double));
  for (ThreadIdType workUnitID = 0; workUnitID < this->m_NumberOfWorkUnits; ++workUnitID)
  {
    memset(m_PerThread[workUnitID].m_MSEDerivative.data_block(), 0, this->m_NumberOfParameters * sizeof(double));
  }


  // MUST BE CALLED TO INITIATE PROCESSING
  this->GetValueAndDerivativeMultiThreadedInitiate();

  itkDebugMacro("Ratio of voxels mapping into moving image buffer: " << this->m_NumberOfPixelsCounted << " / "
                                                                     << this->m_NumberOfFixedImageSamples << std::endl);

  if (this->m_NumberOfPixelsCounted < this->m_NumberOfFixedImageSamples / 4)
  {
    itkExceptionMacro("Too many samples map outside moving image buffer: "
                      << this->m_NumberOfPixelsCounted << " / " << this->m_NumberOfFixedImageSamples << std::endl);
  }

  value = 0;
  for (unsigned int t = 0; t < this->m_NumberOfWorkUnits; ++t)
  {
    value += m_PerThread[t].m_MSE;
    for (unsigned int parameter = 0; parameter < this->m_NumberOfParameters; ++parameter)
    {
      derivative[parameter] += m_PerThread[t].m_MSEDerivative[parameter];
    }
  }

  value /= this->m_NumberOfPixelsCounted;
  for (unsigned int parameter = 0; parameter < this->m_NumberOfParameters; ++parameter)
  {
    derivative[parameter] /= this->m_NumberOfPixelsCounted;
  }
}

template <typename TFixedImage, typename TMovingImage>
void
MeanSquaresImageToImageMetric<TFixedImage, TMovingImage>::GetDerivative(const ParametersType & parameters,
                                                                        DerivativeType &       derivative) const
{
  if (!this->m_FixedImage)
  {
    itkExceptionMacro("Fixed image has not been assigned");
  }

  MeasureType value;
  // call the combined version
  this->GetValueAndDerivative(parameters, value, derivative);
}

} // end namespace itk

#endif
