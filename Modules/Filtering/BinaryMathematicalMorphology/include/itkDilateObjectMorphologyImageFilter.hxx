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
#ifndef itkDilateObjectMorphologyImageFilter_hxx
#define itkDilateObjectMorphologyImageFilter_hxx


namespace itk
{
template <typename TInputImage, typename TOutputImage, typename TKernel>
DilateObjectMorphologyImageFilter<TInputImage, TOutputImage, TKernel>::DilateObjectMorphologyImageFilter()
{
  m_DilateBoundaryCondition.SetConstant(NumericTraits<PixelType>::NonpositiveMin());
  this->OverrideBoundaryCondition(&m_DilateBoundaryCondition);
}

template <typename TInputImage, typename TOutputImage, typename TKernel>
void
DilateObjectMorphologyImageFilter<TInputImage, TOutputImage, TKernel>::Evaluate(OutputNeighborhoodIteratorType & nit,
                                                                                const KernelType &               kernel)
{
  bool valid = true;
  {
    KernelIteratorType       kernel_it = kernel.Begin();
    const KernelIteratorType kernelEnd = kernel.End();
    for (unsigned int i = 0; kernel_it < kernelEnd; ++kernel_it, ++i)
    {
      if (*kernel_it)
      {
        nit.SetPixel(i, this->GetObjectValue(), valid);
      }
    }
  }
}

template <typename TInputImage, typename TOutputImage, typename TKernel>
void
DilateObjectMorphologyImageFilter<TInputImage, TOutputImage, TKernel>::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);
}
} // end namespace itk
#endif
