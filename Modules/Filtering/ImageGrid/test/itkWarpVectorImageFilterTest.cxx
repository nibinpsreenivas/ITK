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

#include <iostream>

#include "itkWarpVectorImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkStreamingImageFilter.h"
#include "itkTestingMacros.h"

// class to produce a linear image pattern
template <int VDimension>
class ImagePattern
{
public:
  using IndexType = itk::Index<VDimension>;
  using SizeType = itk::Size<VDimension>;

  ImagePattern()
  {
    m_Offset = 0.0;
    for (int j = 0; j < VDimension; ++j)
    {
      m_Coeff[j] = 0.0;
    }
  }

  double
  Evaluate(const IndexType & index, const SizeType & size, const SizeType & clampSize, const float padValue)
  {
    double accum = m_Offset;
    for (int j = 0; j < VDimension; ++j)
    {
      if (static_cast<unsigned int>(index[j]) < size[j])
      {
        if (static_cast<unsigned int>(index[j]) >= clampSize[j])
        {
          // Interpolators behave this way in half-pixel band at image perimeter
          accum += m_Coeff[j] * static_cast<double>(clampSize[j] - 1);
        }
        else
        {
          accum += m_Coeff[j] * static_cast<double>(index[j]);
        }
      }
      else
      {
        accum = padValue;
        break;
      }
    }

    return accum;
  }

  double m_Coeff[VDimension];
  double m_Offset;
};


// The following three classes are used to support callbacks
// on the filter in the pipeline that follows later
class ShowProgressObject
{
public:
  ShowProgressObject(itk::ProcessObject * o) { m_Process = o; }

  void
  ShowProgress()
  {
    std::cout << "Progress " << m_Process->GetProgress() << std::endl;
  }

  itk::ProcessObject::Pointer m_Process;
};

int
itkWarpVectorImageFilterTest(int, char *[])
{
  constexpr unsigned int ImageDimension = 2;

  using VectorType = itk::Vector<float, ImageDimension>;
  using FieldType = itk::Image<VectorType, ImageDimension>;

  // In this case, the image to be warped is also a vector field.
  using ImageType = FieldType;
  using PixelType = ImageType::PixelType;
  using IndexType = ImageType::IndexType;

  bool testPassed = true;

  std::cout << "Create the input image pattern." << std::endl;
  ImageType::RegionType region;
  ImageType::SizeType   size = { { 64, 64 } };
  region.SetSize(size);

  auto input = ImageType::New();
  input->SetLargestPossibleRegion(region);
  input->SetBufferedRegion(region);
  input->Allocate();

  ImagePattern<ImageDimension> pattern;
  pattern.m_Offset = 64;
  for (unsigned int j = 0; j < ImageDimension; ++j)
  {
    pattern.m_Coeff[j] = 1.0;
  }

  using Iterator = itk::ImageRegionIteratorWithIndex<ImageType>;

  constexpr float padValue = 4.0;

  for (Iterator inIter(input, region); !inIter.IsAtEnd(); ++inIter)
  {
    inIter.Set(PixelType(pattern.Evaluate(inIter.GetIndex(), size, size, padValue)));
  }

  std::cout << "Create the input displacement field." << std::endl;

  constexpr unsigned int factors[ImageDimension] = { 2, 3 };

  ImageType::RegionType fieldRegion;
  ImageType::SizeType   fieldSize;
  for (unsigned int j = 0; j < ImageDimension; ++j)
  {
    fieldSize[j] = size[j] * factors[j] + 5;
  }
  fieldRegion.SetSize(fieldSize);

  auto field = FieldType::New();
  field->SetLargestPossibleRegion(fieldRegion);
  field->SetBufferedRegion(fieldRegion);
  field->Allocate();

  using FieldIterator = itk::ImageRegionIteratorWithIndex<FieldType>;

  for (FieldIterator fieldIter(field, fieldRegion); !fieldIter.IsAtEnd(); ++fieldIter)
  {
    IndexType  index = fieldIter.GetIndex();
    VectorType displacement;
    for (unsigned int j = 0; j < ImageDimension; ++j)
    {
      displacement[j] = static_cast<float>(index[j]) * ((1.0 / factors[j]) - 1.0);
    }
    fieldIter.Set(displacement);
  }

  std::cout << "Run WarpVectorImageFilter in standalone mode with progress." << std::endl;
  using WarperType = itk::WarpVectorImageFilter<ImageType, ImageType, FieldType>;
  auto warper = WarperType::New();

  ITK_EXERCISE_BASIC_OBJECT_METHODS(warper, WarpVectorImageFilter, ImageToImageFilter);


  warper->SetInput(input);
  warper->SetDisplacementField(field);
  ITK_TEST_SET_GET_VALUE(field, warper->GetDisplacementField());

  warper->SetEdgePaddingValue(PixelType(padValue));
  ITK_TEST_SET_GET_VALUE(PixelType(padValue), warper->GetEdgePaddingValue());

  ShowProgressObject                                          progressWatch(warper);
  const itk::SimpleMemberCommand<ShowProgressObject>::Pointer command =
    itk::SimpleMemberCommand<ShowProgressObject>::New();
  command->SetCallbackFunction(&progressWatch, &ShowProgressObject::ShowProgress);
  warper->AddObserver(itk::ProgressEvent(), command);

  auto array = itk::MakeFilled<itk::FixedArray<double, ImageDimension>>(2.0);
  warper->SetOutputSpacing(array.GetDataPointer());
  ITK_TEST_SET_GET_VALUE(array, warper->GetOutputSpacing());

  array.Fill(1.0);
  warper->SetOutputSpacing(array.GetDataPointer());
  ITK_TEST_SET_GET_VALUE(array, warper->GetOutputSpacing());

  auto ptarray = itk::MakeFilled<WarperType::PointType>(-10.0);
  warper->SetOutputOrigin(ptarray.GetDataPointer());
  ITK_TEST_SET_GET_VALUE(ptarray, warper->GetOutputOrigin());

  ptarray.Fill(0.0);
  warper->SetOutputOrigin(ptarray.GetDataPointer());
  ITK_TEST_SET_GET_VALUE(ptarray, warper->GetOutputOrigin());

  typename WarperType::DirectionType outputDirection;
  outputDirection.SetIdentity();
  warper->SetOutputDirection(outputDirection);
  ITK_TEST_SET_GET_VALUE(outputDirection, warper->GetOutputDirection());

  // Update the filter
  warper->Update();

  std::cout << "Checking the output against expected." << std::endl;

  // compute non-padded output region
  ImageType::RegionType validRegion;
  ImageType::SizeType   validSize = validRegion.GetSize();
  // Needed to deal with incompatibility of various IsInside()s &
  // nearest-neighbour type interpolation on half-band at perimeter of
  // image. Evaluate() now has logic for this outer half-band.
  ImageType::SizeType decrementForScaling;
  ImageType::SizeType clampSizeDecrement;
  ImageType::SizeType clampSize;
  for (unsigned int j = 0; j < ImageDimension; ++j)
  {
    validSize[j] = size[j] * factors[j];

    // Consider as inside anything < 1/2 pixel of (size[j]-1)*factors[j]
    //(0-63) map to (0,126), with 127 exactly at 1/2 pixel, therefore
    // edged out; or to (0,190), with 190 just beyond 189 by 1/3 pixel;
    // or to (0,253), with 254 exactly at 1/2 pixel, therefore out
    // also; or (0, 317), with 317 at 2/5 pixel beyond 315. And so on.

    decrementForScaling[j] = factors[j] / 2;

    validSize[j] -= decrementForScaling[j];

    // This part of logic determines what is inside, but in outer
    // 1/2 pixel band, which has to be clamped to that nearest outer
    // pixel scaled by factor: (0,63) maps to (0,190) as inside, but
    // pixel 190 is outside of (0,189), and must be clamped to it.
    // If factor is 2 or less, this decrement has no effect.

    if (factors[j] < 1 + decrementForScaling[j])
    {
      clampSizeDecrement[j] = 0;
    }
    else
    {
      clampSizeDecrement[j] = (factors[j] - 1 - decrementForScaling[j]);
    }
    clampSize[j] = validSize[j] - clampSizeDecrement[j];
  }
  validRegion.SetSize(validSize);

  // adjust the pattern coefficients to match
  for (unsigned int j = 0; j < ImageDimension; ++j)
  {
    pattern.m_Coeff[j] /= static_cast<double>(factors[j]);
  }

  Iterator outIter(warper->GetOutput(), warper->GetOutput()->GetBufferedRegion());
  while (!outIter.IsAtEnd())
  {
    const IndexType index = outIter.GetIndex();
    PixelType       value = outIter.Get();

    if (validRegion.IsInside(index))
    {
      PixelType trueValue(pattern.Evaluate(outIter.GetIndex(), validSize, clampSize, padValue));
      for (unsigned int k = 0; k < ImageDimension; ++k)
      {
        if (itk::Math::abs(trueValue[k] - value[k]) > 1e-4)
        {
          std::cerr << "Test failed!" << std::endl;
          std::cerr << "Error in Evaluate at index [" << index << "]" << std::endl;
          std::cerr << "Expected value " << trueValue << std::endl;
          std::cerr << " differs from " << value << std::endl;
          testPassed = false;
          break;
        }
      }
    }
    else
    {
      if (value != PixelType(padValue))
      {
        std::cerr << "Test failed!" << std::endl;
        std::cerr << "Error in Evaluate at index [" << index << "]" << std::endl;
        std::cerr << "Expected value " << padValue << std::endl;
        std::cerr << " differs from " << value << std::endl;
        testPassed = false;
      }
    }
    ++outIter;
  }

  std::cout << "Run ExpandImageFilter with streamer" << std::endl;

  using VectorCasterType = itk::CastImageFilter<FieldType, FieldType>;
  auto vcaster = VectorCasterType::New();

  vcaster->SetInput(warper->GetDisplacementField());

  auto warper2 = WarperType::New();

  warper2->SetInput(warper->GetInput());
  warper2->SetDisplacementField(vcaster->GetOutput());
  warper2->SetEdgePaddingValue(warper->GetEdgePaddingValue());

  using StreamerType = itk::StreamingImageFilter<ImageType, ImageType>;
  auto streamer = StreamerType::New();
  streamer->SetInput(warper2->GetOutput());
  streamer->SetNumberOfStreamDivisions(3);
  streamer->Update();

  std::cout << "Compare standalone and streamed outputs" << std::endl;

  Iterator streamIter(streamer->GetOutput(), streamer->GetOutput()->GetBufferedRegion());

  outIter.GoToBegin();
  streamIter.GoToBegin();

  while (!outIter.IsAtEnd())
  {
    if (outIter.Get() != streamIter.Get())
    {
      std::cerr << "Test failed!" << std::endl;
      std::cerr << "Error in streamed output at index [" << outIter.GetIndex() << "]" << std::endl;
      std::cerr << "Expected value " << outIter.Get() << std::endl;
      std::cerr << " differs from " << streamIter.Get() << std::endl;
      testPassed = false;
    }
    ++outIter;
    ++streamIter;
  }


  if (!testPassed)
  {
    std::cout << "Test failed." << std::endl;
    return EXIT_FAILURE;
  }

  // Exercise error handling

  using InterpolatorType = WarperType::InterpolatorType;
  const InterpolatorType::Pointer interp = warper->GetModifiableInterpolator();

  std::cout << "Setting interpolator to nullptr" << std::endl;
  warper->SetInterpolator(nullptr);

  ITK_TRY_EXPECT_EXCEPTION(warper->Update());

  warper->ResetPipeline();
  warper->SetInterpolator(interp);

  ITK_TEST_SET_GET_VALUE(interp, warper->GetInterpolator());


  std::cout << "Test finished." << std::endl;
  return EXIT_SUCCESS;
}
