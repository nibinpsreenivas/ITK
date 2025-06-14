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

#include "itkSliceBySliceImageFilter.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkSimpleFilterWatcher.h"
#include "itkPipelineMonitorImageFilter.h"
#include "itkTestingMacros.h"
#include "itkMedianImageFilter.h"


void
sliceCallBack(itk::Object * object, const itk::EventObject &, void *)
{
  // the same type alias than in the main function - should be done in a nicer way
  constexpr int Dimension = 3;
  using PixelType = unsigned char;

  using ImageType = itk::Image<PixelType, Dimension>;
  using FilterType = itk::SliceBySliceImageFilter<ImageType, ImageType>;
  using MedianType = itk::MedianImageFilter<FilterType::InternalInputImageType, FilterType::InternalOutputImageType>;

  // real stuff begins here
  // get the slice by slice filter and the median filter
  auto * filter = dynamic_cast<FilterType *>(object);
  auto * median = dynamic_cast<MedianType *>(filter->GetModifiableInputFilter());

  // std::cout << "callback! slice: " << filter->GetSliceIndex() << std::endl;

  // set half of the slice number as radius
  auto radius = MedianType::InputSizeType::Filled(filter->GetSliceIndex() / 2);
  median->SetRadius(radius);
}

int
itkSliceBySliceImageFilterTest(int argc, char * argv[])
{

  if (argc != 4)
  {
    std::cerr << "usage: " << itkNameOfTestExecutableMacro(argv) << " input output slicingDimension" << std::endl;
    return EXIT_FAILURE;
  }

  constexpr int Dimension = 3;
  using PixelType = unsigned char;

  using ImageType = itk::Image<PixelType, Dimension>;

  using ReaderType = itk::ImageFileReader<ImageType>;

  auto reader = ReaderType::New();
  reader->SetFileName(argv[1]);

  using FilterType = itk::SliceBySliceImageFilter<ImageType, ImageType>;

  auto filter = FilterType::New();

  ITK_EXERCISE_BASIC_OBJECT_METHODS(filter, SliceBySliceImageFilter, ImageToImageFilter);


  filter->DebugOn();

  filter->SetInput(reader->GetOutput());

  using MedianType = itk::MedianImageFilter<FilterType::InternalInputImageType, FilterType::InternalOutputImageType>;

  auto median = MedianType::New();
  filter->SetFilter(median);
  ITK_TEST_SET_GET_VALUE(median, filter->GetFilter());


  using MonitorType = itk::PipelineMonitorImageFilter<FilterType::InternalOutputImageType>;
  auto monitor = MonitorType::New();

  const itk::CStyleCommand::Pointer command = itk::CStyleCommand::New();
  command->SetCallback(*sliceCallBack);

  filter->AddObserver(itk::IterationEvent(), command);

  const itk::SimpleFilterWatcher watcher(filter, "filter");

  using WriterType = itk::ImageFileWriter<ImageType>;

  auto writer = WriterType::New();
  writer->SetInput(filter->GetOutput());
  writer->SetFileName(argv[2]);

  unsigned int       slicingDimension = 0;
  std::istringstream istrm(argv[3]);
  istrm >> slicingDimension;
  filter->SetDimension(slicingDimension);
  ITK_TEST_SET_GET_VALUE(slicingDimension, filter->GetDimension());

  try
  {
    writer->Update();
  }
  catch (const itk::ExceptionObject & excp)
  {
    std::cerr << excp << std::endl;
    return EXIT_FAILURE;
  }


  // set up a requested region of just one pixel and verify that was
  // all that was produced.
  std::cout << "Testing with requested region..." << std::endl;
  ImageType::Pointer temp = filter->GetOutput();
  temp->DisconnectPipeline();
  temp = nullptr;

  ImageType::RegionType rr = reader->GetOutput()->GetLargestPossibleRegion();
  for (unsigned int i = 0; i < ImageType::ImageDimension; ++i)
  {
    rr.SetIndex(i, rr.GetIndex(i) + rr.GetSize(i) / 2);
    rr.SetSize(i, 1);
  }


  monitor->SetInput(median->GetOutput());

  filter->SetOutputFilter(monitor);
  ITK_TEST_SET_GET_VALUE(monitor, filter->GetOutputFilter());

  filter->GetOutput()->SetRequestedRegion(rr);


  try
  {
    filter->Update();
  }
  catch (const itk::ExceptionObject & excp)
  {
    std::cerr << excp << std::endl;
    return EXIT_FAILURE;
  }

  // check that one slice executed is just one pixel and the input
  // filter just update that region
  ITK_TEST_EXPECT_EQUAL(monitor->GetNumberOfUpdates(), 1);
  ITK_TEST_EXPECT_EQUAL(monitor->GetOutputRequestedRegions()[0].GetNumberOfPixels(), 1);
  ITK_TEST_EXPECT_TRUE(monitor->VerifyAllInputCanStream(1));

  //
  // Test that a sliced version of the input image information is passed
  // through to the internal filters, with proper origin and
  // spacing. We are setting the input image to have a non-zero
  // starting index.
  //
  auto image = ImageType::New();
  {
    ImageType::RegionType region = reader->GetOutput()->GetLargestPossibleRegion();
    region.SetIndex(0, 10);
    image->SetRegions(region);
    image->AllocateInitialized();
  }

  ImageType::SpacingType spacing;
  ImageType::PointType   origin;
  for (unsigned int i = 0; i < ImageType::ImageDimension; ++i)
  {
    spacing[i] = i + 0.1;
    origin[i] = i + 0.2;
  }
  image->SetSpacing(spacing);
  image->SetOrigin(origin);

  filter->SetInput(image);
  filter->Update();

  FilterType::InternalInputImageType::SpacingType expectedInternalSpacing;
  FilterType::InternalInputImageType::PointType   expectedInternalOrigin;
  for (unsigned int i = 0, internal_i = 0; internal_i < FilterType::InternalImageDimension; ++i, ++internal_i)
  {
    if (i == slicingDimension)
    {
      ++i;
    }

    expectedInternalSpacing[internal_i] = spacing[i];
    expectedInternalOrigin[internal_i] = origin[i];
  }
  ITK_TEST_EXPECT_EQUAL(monitor->GetUpdatedOutputSpacing(), expectedInternalSpacing);
  ITK_TEST_EXPECT_EQUAL(monitor->GetUpdatedOutputOrigin(), expectedInternalOrigin);

  //
  // Exercise exceptions
  //
  auto badFilter = FilterType::New();

  std::cout << "Testing with no filter set..." << std::endl;
  badFilter->SetInput(reader->GetOutput());
  bool caughtException = false;
  try
  {
    badFilter->Update();
  }
  catch (const itk::ExceptionObject & excp)
  {
    std::cout << "Caught expected exception" << std::endl;
    std::cout << excp << std::endl;
    caughtException = true;
  }
  if (!caughtException)
  {
    return EXIT_FAILURE;
  }

  std::cout << "Testing with no output filter set..." << std::endl;
  badFilter->SetInput(reader->GetOutput());

  badFilter->SetInputFilter(median);
  ITK_TEST_SET_GET_VALUE(median, badFilter->GetInputFilter());

  caughtException = false;
  try
  {
    badFilter->Update();
  }
  catch (const itk::ExceptionObject & excp)
  {
    std::cout << "Caught expected exception" << std::endl;
    std::cout << excp << std::endl;
    caughtException = true;
  }
  if (!caughtException)
  {
    return EXIT_FAILURE;
  }

  // check nullptr input/output
  ITK_TRY_EXPECT_EXCEPTION(badFilter->SetInputFilter(nullptr));
  ITK_TRY_EXPECT_EXCEPTION(badFilter->SetOutputFilter(nullptr));

  return EXIT_SUCCESS;
}
